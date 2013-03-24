// Copyright (c) 2009 Gratian Lup. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// * The name "ParallelAllocator" must not be used to endorse or promote
// products derived from this software without prior written permission.
//
// * Products derived from this software may not be called "ParallelAllocator" nor
// may "ParallelAllocator" appear in their names without prior written
// permission of the author.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Implements the main allocator module that acts as an interface with the clients.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_HPP
#define PC_BASE_ALLOCATOR_HPP

#include "Group.hpp"
#include "Bitmap.hpp"
#include "BlockAllocator.hpp"
#include "ThreadUtils.hpp"
#include "AllocatorConstants.hpp"
#include "Atomic.hpp"
#include "HugeLocation.hpp"
#include "LargeGroup.hpp"
#include "BasicMemory.hpp"
#include "NumaMemory.hpp"
#include <math.h>

#if defined(PLATFORM_WINDOWS)
    #include <Windows.h>
    #include <Psapi.h>
#else
    static_assert(false, "Not yet implemented.");
#endif

namespace Base {

class Allocator {
private:
    // Nested types
    #pragma pack(push)
    #pragma pack(1) // Make sure the compiler doesn't change the layout of the structures.
    struct BinHeader {
        unsigned __int64 AvailableGroups;
        unsigned int UsedBins;
        
        // Padding to cache line.
        char Padding[Constants::CACHE_LINE_SIZE - 
                     sizeof(unsigned __int64) - sizeof(unsigned int)];
    };

    template <class NodeType, class PolicyType>
    struct Bin : public ObjectList<NodeType, PolicyType> {
        typedef typename ObjectList<NodeType, PolicyType> ListType;

        NodeType* PublicGroup;
        NodeType* StolenGroup;
        unsigned int ReturnAllowed;
        unsigned int CanReturnPartial;
        unsigned int Number;
        unsigned int PublicLock;
        unsigned int CanSteal;
        unsigned int StolenLocations;
        unsigned int MaxStolenLocations;

        // Padding to cache line.
        char Padding[Constants::CACHE_LINE_SIZE - sizeof(ListType) - 
                    (2 * sizeof(void*)) - (7 * sizeof(unsigned int))];
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    typedef Bin<typename SmallTraits::NodeType, typename SmallTraits::PolicyType> SmallBin;
    typedef Bin<typename LargeTraits::NodeType, typename LargeTraits::PolicyType> LargeBin;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Each thread that made an allocation has an associated context
    // that is retrieved/set through TLS.
    struct ThreadContext {
        unsigned int ThreadId;
        unsigned int HugeOperations;
        unsigned int NumaNode; // The node where this thread was first used.

        // Padding to cache line.
        char Padding[Constants::CACHE_LINE_SIZE - (3 * sizeof(unsigned int))];

        BinHeader Header;
        SmallBin SmallBins[Constants::SMALL_BINS];
        LargeBin LargeBins[Constants::LARGE_BINS];
    };
    #pragma pack(pop) // Restore the original alignment.


    // Arguments for the threads that cleans the cache with huge locations.
    struct CacheThreadArgs {
        void* ThreadHandle;
        Allocator* Allocator;
        unsigned int Timeout;
    };
    

    // Used under Linux for locations that exceed the size
    // that can be handled by the allocator (> 1MB).
    struct OSHeader {
        void* RealAddress;     // The address returned by the OS.
        void* LocationAddress; // The address of the user location (after this header).

#if defined(PLATFORM_32)
        char Padding[8];       // The location should be aligned to a 16 byte boundary.
#endif
    };


    // Used to select between the two available 
    // memory allocation systems (basic and NUMA).
    template <class T, class U, bool IsNuma>
    struct MemoryPolicySelector {
        typedef typename BasicMemory<T, U> PolicyType;
    };

    template <class T, class U>
    struct MemoryPolicySelector<T, U, true> {
        typedef typename NumaMemory<T, U> PolicyType;
    };
    
public:
    typedef BlockAllocator<Constants::SMALL_BINS, Constants::BLOCK_SIZE, 
                           Constants::SMALL_GROUP_SIZE, Constants::BLOCK_SMALL_CACHE, 
                           Group, SmallBin, SmallTraits> SmallBAType;
    
    typedef BlockAllocator<Constants::LARGE_BINS, Constants::BLOCK_SIZE, 
                           Constants::LARGE_GROUP_SIZE, Constants::BLOCK_LARGE_CACHE, 
                           LargeGroup, LargeBin, LargeTraits> LargeBAType;

    typedef MemoryPolicySelector<SmallBAType, LargeBAType, 
                                 Constants::NUMA_ENABLED>::PolicyType MemoryPolicy;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    volatile bool initialized_;
    volatile bool cacheThreadInitialized_;
    unsigned int initLock_; // Used for the initialization of the allocator.
    unsigned int cacheThreadLock_;
    unsigned int tlsIndex_; // The index used by all threads to store their context.

    MemoryPolicy memoryPolicy_;
    SmallBAType* smallBlockAlloc_[Constants::MAX_NUMA_NODES];
    LargeBAType* largeBlockAlloc_[Constants::MAX_NUMA_NODES];
    ObjectPool threadContextPool_;  // Used to allocate thread context objects.
    ObjectPool blockAllocatorPool_; // Used to allocate block allocators for each NUMA node.
    HugeBin hugeBins_[Constants::HUGE_BINS]; // Keeps track of freed (unused) huge locations.

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Provides access to group-specific data, based on the type 
    // of the group (small or large). Selector for small groups.
    template <class T>
    struct Selector {
        typedef SmallBAType BAType;
        typedef SmallBin BinType;
        typedef Group GroupType;

        static const unsigned int GroupSize  = Constants::SMALL_GROUP_SIZE;
        static const unsigned int HeaderSize = Constants::SMALL_GROUP_HEADER_SIZE;

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        static SmallBAType* GetBA(Allocator* allocator, unsigned int node) {
            return allocator->smallBlockAlloc_[node];
        }

        static BinType* GetBin(ThreadContext* context, unsigned int index) {
            return& context->SmallBins[index];
        }

        static void GetAllocInfo(Allocator* alloc, size_t size, 
                                 AllocationInfo& allocInfo) {
            alloc->GetAllocationInfoSmall(size, allocInfo);
        }

        static bool CanReturnPartial(BinType* bin) {
            return bin->CanReturnPartial;
        }
    };


    // Selector for large groups.
    template <>
    struct Selector<LargeBAType> {
        typedef LargeBAType BAType;
        typedef LargeBin BinType;
        typedef LargeGroup GroupType;

        static const unsigned int GroupSize  = Constants::LARGE_GROUP_SIZE;
        static const unsigned int HeaderSize = Constants::LARGE_GROUP_HEADER_SIZE;

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        static LargeBAType* GetBA(Allocator* allocator, unsigned int node) {
            return allocator->largeBlockAlloc_[node];
        }

        static BinType* GetBin(ThreadContext* context, unsigned int index) {
            return& context->LargeBins[index];
        }

        static void GetAllocInfo(Allocator* alloc, size_t size,
                                 AllocationInfo& allocInfo) {
            alloc->GetAllocationInfoLarge(size, allocInfo);
        }

        static bool CanReturnPartial(BinType* bin) {
            return true;
        }
    };
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void Initialize()	{
        // Uses the double-checked locking, corrected for multicore
        // (see Andrei Alexandrescu - C++ and the Perils of Double-Checked Locking).
        bool state = Memory::ReadValue(&initialized_);

        if(!state) {
            // Acquire the lock. Will be automatically released by the destructor.
            SpinLock lock(&initLock_);

            if(!initialized_) {
                // Get a slot in the TLS array.
                tlsIndex_ = ThreadUtils::AllocateTLSIndex();

                // Make sure that the flag is set
                // only after the TLS index was allocated.
                Memory::WriteValue(&initialized_, true);
            }
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the context associated with this thread from TLS.
    ThreadContext* GetCurrentContext() {
        return reinterpret_cast<ThreadContext*>(ThreadUtils::GetTLSValue(tlsIndex_));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Creates and initializes a new context, and if needed, initializes TLS.
    ThreadContext* CreateContext() {
        Statistics::ThreadCreated();

        // Make sure the allocator is initialized_.
        Initialize();

        // A context needs to be created for this thread.
        auto context = reinterpret_cast<ThreadContext*>(threadContextPool_.GetObject());

        // The constructor needs to be called because the contexts
        // can be reused after they are no longer needed.
        new(context) ThreadContext(); 
        context->ThreadId = ThreadUtils::GetCurrentThreadId();
        context->HugeOperations = 0;

#if defined(PLATFORM_NUMA)
        // Assign the NUMA node.
        context->NumaNode = memoryPolicy_.GetCpuNode(ThreadUtils::GetCurrentCPUNumber());
#else
        context->NumaNode = 0;
#endif
        ThreadUtils::SetTLSValue(tlsIndex_, context);

        // Initialize the bins.
        for(unsigned int i = 0; i < Constants::SMALL_BINS; i++) {
            SmallBin* bin = &context->SmallBins[i];
            bin->Number = i;
            bin->ReturnAllowed = 1;
            bin->PublicGroup = nullptr;
            bin->CanReturnPartial = Bitmap::IsBitSet(Constants::GROUP_RETURN_PARTIAL, i);

#if defined(STEAL)
            bin->CanSteal = 1;
            bin->MaxStolenLocations = (Constants::SMALL_GROUP_SIZE / 
                                       Constants::SmallBinSize[i]) / 2;
#endif
        }

        for(unsigned int i = 0; i < Constants::LARGE_BINS; i++) {
            LargeBin* bin = &context->LargeBins[i];
            bin->Number = i;
            bin->ReturnAllowed = 1;
            bin->PublicGroup = nullptr;

#if defined(STEAL)
            bin->CanSteal = 1;
            bin->MaxStolenLocations = (Constants::LARGE_GROUP_SIZE / 
                                       Constants::LargeBinSize[i]) / 2;
#endif
        }

        return context;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the specified context to the context pool.
    void ReleaseContext(ThreadContext* context) {
        ThreadUtils::SetTLSValue(tlsIndex_, nullptr);
        threadContextPool_.ReturnObject(context);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Makes the specified group the active one.
    template <class GroupType, class BinType>
    void AddNewGroup(BinType* bin, GroupType* group) {
        bin->AddFirst(group);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Brings the specified group to the front of the bin.
    template <class GroupType, class BinType>
    void MakeGroupActive(BinType* bin, GroupType* group) {
        // Bring the group to the front of the list.
        unsigned int prev = bin->Count();
        GroupType* activeGroup = static_cast<GroupType*>(bin->First());

        bin->RemoveFirst();
        bin->AddLast(activeGroup);

        if(bin->First() != group) {
            // The group is not the first in the list yet.
            bin->Remove(group);
            bin->AddFirst(group);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Determines the required allocation size and bin for small locations.
    // Sizes under 64 bytes are determined using a lockup table.
    // Sizes between 64 and 1024 bytes are computed.
    // Sizes above 1024 bytes are selected using if-else statements.
    void GetAllocationInfoSmall(size_t size, AllocationInfo& allocInfo) {
        if(size <= Constants::MAX_TINY_SIZE) {
            // Using a lookup table seems to be much faster 
            // than the jump table generated by a switch statement.
            allocInfo = Constants::SmallAllocTable[size];
        }
        else if(size <= Constants::MAX_SEGREGATED_SIZE) {
            // If the size is below 1024, the information 
            // can be computed  and no lookup table is required 
            // (it would be very large and cause cache misses).
            unsigned int highestBit = Bitmap::SearchReverse((unsigned int)size - 1);

            // Between two consecutive powers of two there are 3 other bins, 
            // spread uniformly, with sizes also power of two. 
            // Round the size to the nearest greater number that is a power of two.
            // 127 is the maximum distance between the bins.
            unsigned int offset = 127 >> (9 - highestBit); 
            allocInfo.Size = (size + offset) & ~offset;
            allocInfo.Bin = ((size - 1) >> (highestBit - 2)) + (4*  (highestBit - 5)) + 3;
        }
        else if(size <= Constants::MAX_SMALL_SIZE) {
            /*// Search for the allocation size and bin using binary search.
            if(size <= Constants::ALLOCATION_SIZE_5)
            {
                if(size <= Constants::ALLOCATION_SIZE_3)
                {
                    if(size <= Constants::ALLOCATION_SIZE_2)
                    {
                        if(size <= Constants::ALLOCATION_SIZE_1)
                        {
                            allocInfo.Size = Constants::ALLOCATION_SIZE_1;
                            allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 0;
                        }
                        else
                        {
                            allocInfo.Size = Constants::ALLOCATION_SIZE_2;
                            allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 1;
                        }
                    }
                    else
                    {
                        allocInfo.Size = Constants::ALLOCATION_SIZE_3;
                        allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 2;
                    }
                }
                else
                {
                    if(size <= Constants::ALLOCATION_SIZE_4)
                    {
                        allocInfo.Size = Constants::ALLOCATION_SIZE_4;
                        allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 3;
                    }
                    else
                    {
                        allocInfo.Size = Constants::ALLOCATION_SIZE_5;
                        allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 4;
                    }
                }
            }
            else
            {
                if(size <= Constants::ALLOCATION_SIZE_7)
                {
                    if(size <= Constants::ALLOCATION_SIZE_6)
                    {
                        if(size <= Constants::ALLOCATION_SIZE_5)
                        {
                            allocInfo.Size = Constants::ALLOCATION_SIZE_5;
                            allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 4;
                        }
                        else
                        {
                            allocInfo.Size = Constants::ALLOCATION_SIZE_6;
                            allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 5;
                        }
                    }
                    else
                    {
                        allocInfo.Size = Constants::ALLOCATION_SIZE_7;
                        allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 6;
                    }
                }
                else
                {
                    if(size <= Constants::ALLOCATION_SIZE_8)
                    {
                        allocInfo.Size = Constants::ALLOCATION_SIZE_8;
                        allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 7;
                    }
                    else
                    {
                        allocInfo.Size = Constants::ALLOCATION_SIZE_9;
                        allocInfo.Bin = Constants::AFTER_SEGREGATED_START_BIN + 8;
                    }
                }
            }*/

            // Using a lookup table seems to be about twice as fast than 
            // the "binary search" method. The table is not likely to be held 
            // in cache, because large objects are allocated infrequently, 
            // but it doesn't seem to be an issue even if the whole L1 cache is flushed.
            allocInfo = Constants::SmallAllocTable2[size / 320];
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Determines the required allocation size and bin for large locations.
    void GetAllocationInfoLarge(size_t size, AllocationInfo& allocInfo) {
        // Selects using a series of if-else statements.
        if(size <= Constants::LARGE_ALLOCATION_SIZE_1) {
            allocInfo = AllocationInfo(Constants::LARGE_ALLOCATION_SIZE_1, 0);
        }
        else if(size <= Constants::LARGE_ALLOCATION_SIZE_2) {
            allocInfo = AllocationInfo(Constants::LARGE_ALLOCATION_SIZE_2, 1);
        }
        else if(size <= Constants::LARGE_ALLOCATION_SIZE_3) {
            allocInfo = AllocationInfo(Constants::LARGE_ALLOCATION_SIZE_3, 2);
        }
        else {
            allocInfo = AllocationInfo(Constants::LARGE_ALLOCATION_SIZE_4, 3);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsHugeLocation(void* address, void* aligned) {
        // Huge locations always start at 64 bytes, 
        // relative to the group alignment (16 KB).
        return ((uintptr_t)address - (uintptr_t)aligned) <= 
                Constants::HUGE_HEADER_SIZE;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsOSLocation(void* address, void* aligned) {
        return ((uintptr_t)address - (uintptr_t)aligned) <= sizeof(OSHeader);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsLargeLocation(void* address, void* aligned) {
        return LargeTraits::PolicyType::
            GetType(reinterpret_cast<LargeTraits::NodeType*>(aligned)) != 0;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to steal a mostly-empty group from another bin.
    template <class Manager>
    typename Selector<Manager>::GroupType* 
    StealGroup(ThreadContext* context, unsigned int startBin) {
        typedef typename Selector<Manager> GS; // Group selector.

        // Get the index of the first bin that has a (mostly) empty active group.
        // If the found group is not empty enough, continue searching until 
        // a suitable group is found, or we reach the last bin.
        while(startBin < Constants::SMALL_BINS) {
            unsigned int index = Bitmap::SearchForward(context->Header.AvailableGroups, startBin);
            
            if(index != -1) {
                auto groupObject = GS::GetBin(context, index)->First;
                GS::GroupType* group = static_cast<GS::GroupType*>(groupObject);

                // Need to recheck because the status is updated only when
                // the group is initialized_ or made active.
                if(group->CanBeStolen()) {
                    return group;
                }

                // Next time search from the returned index.
                startBin = index + 1;
            }

            break; // No bin with available locations could be found.
        }

        return nullptr;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Removes the specified group from all the bins that come before the owner one.
    template <class Manager>
    void RemoveStolenGroup(ThreadContext* context, typename Selector<Manager>::GroupType* group, 
                           unsigned int groupBin) {
        typedef typename Selector<Manager> GC; // Group context.

        if(group->SmallestStolen == Constants::NOT_STOLEN) {
            // This group hasn't been stolen yet.
            return;
        }

        // Search all bins before the owner of the group
        // and reset the 'StolenGroup' pointer. 
        // Need to search only between 'startBin' and 'groupBin'.
        unsigned int startBin = group->SmallestStolen;

        for(unsigned int i = startBin; i < groupBin; i++) {
            GC::BinType* bin = &context->SmallBins[i];

            if(bin->StolenGroup == group) {
                // The group has been stolen by this bin, don't let it anymore.
                bin->StolenGroup = nullptr;
            }
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <>
    void RemoveStolenGroup<LargeBAType>(ThreadContext* context, LargeGroup* group, 
                                        unsigned int groupBin) {
        // Stealing is always disabled for large groups.
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Marks the specified bin as (un)available for stealing by other bins.
    template <class GroupType>
    void SetAvailableForStealing(ThreadContext* context, unsigned int binIndex,
                                 bool available) {
        if(available) {
            Bitmap::SetBit(context->Header.AvailableGroups, binIndex);
        }
        else Bitmap::ResetBit(context->Header.AvailableGroups, binIndex);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <>
    void SetAvailableForStealing<LargeGroup>(ThreadContext* context,
                                             unsigned int binIndex, bool available) {
        // Stealing always disabled for large groups.
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class Manager>
    void* TrySteal(typename Selector<Manager>::BinType* bin, 
                   ThreadContext* context, AllocationInfo& allocInfo) {
        typedef typename Selector<Manager> GC; // Group context.
        void* address;

        GC::GroupType* stolenGroup = static_cast<GC::GroupType*>(bin->StolenGroup);

        if((stolenGroup == nullptr) && bin->CanSteal) {
            stolenGroup = StealGroup<Manager>(context, bin->Number + 1);

            if(stolenGroup != nullptr) {
                // A group could be stolen and will be now linked 
                // with the current bin. Keep track of the smallest bin 
                // index that stole from this group.
                bin->StolenGroup = stolenGroup;

                if(bin->Number < stolenGroup->SmallestStolen) {
                    stolenGroup->SmallestStolen = bin->Number;
                }
            }
        }

        // A recheck is needed.
        if(stolenGroup != nullptr) { 
            // Allocate from the stolen group.
            address = stolenGroup->StealLocation(allocInfo.Size);

            if(address != nullptr) {
                bin->StolenLocations++;
                bin->CanSteal = bin->StolenLocations < bin->MaxStolenLocations;
                return address;
            }
            else { 
                // This group has no free locations.
                bin->StolenGroup = nullptr;
            }
        }

        return nullptr;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <>
    void* TrySteal<LargeBAType>(LargeBin* bin, ThreadContext* context,
                                AllocationInfo& allocInfo) {
        // Stealing always disabled for large groups.
        return nullptr;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets a location large enough to hold the specified number of bytes.
    template <class Manager>
    void* Allocate(size_t size)	{
        // It tries to obtain the location in the following order:
        // 1. Active group.
        // 2. Make second group active (if it's empty enough).
        // 3. Make a group with freed location by other threads (public) active.
        // 4. Steal a location (if enabled).
        // 5. Get a new (partially)empty group.
        // If none of the above methods finds a location, 
        // the system has run out of memory!
        typedef typename Selector<Manager> GS; // Group selector.

        // Get the context associated with this thread.
        ThreadContext* context = GetCurrentContext();

        if(context == nullptr) {
            context = CreateContext();
        }

        // Get the size and the bin for this allocation.
        AllocationInfo allocInfo;
        GS::GetAllocInfo(this, size, allocInfo);

        // The object is small enough so it will be allocated from a group.
        // Allocate the object from the corresponding bin.
        GS::BinType* bin = GS::GetBin(context, allocInfo.Bin);
        GS::GroupType* activeGroup = static_cast<GS::GroupType*>(bin->First());
        void* address = nullptr;

        // 1. Take from the active group.
        if(activeGroup != nullptr)	{
            address = activeGroup->GetPrivateLocation();
            
            if(address != nullptr) {
                return address;
            }
        }

        // 2. An active bin doesn't exist, or it is full.
        // We see if the next group has free locations. If it does not,
        // it is guaranteed that all the other groups don't have free locations too.
        if(bin->Count() >= 2) {
            auto groupObject = GS::BinType::Policy::GetNext(bin->First());
            activeGroup = static_cast<GS::GroupType*>(groupObject);

            if(activeGroup->IsEmptyEnough()) {
                Statistics::ActiveGroupChanged(activeGroup->Next);

                // Make the second group the active one.
                MakeGroupActive(bin, activeGroup);
#if defined(STEAL)
                SetAvailableForStealing<GS::GroupType>(context, allocInfo.Bin, 
                                                       activeGroup->CanBeStolen());
#endif
                return activeGroup->GetLocation();
            }
        }

        // 3. See if there is any group that has free public locations.
        if(bin->PublicGroup != nullptr) {
            // Synchronize access to the public list.
            SpinLock binLock(&bin->PublicLock);

            activeGroup = static_cast<GS::GroupType*>(bin->PublicGroup);
            bin->PublicGroup = static_cast<GS::GroupType*>(activeGroup->NextPublic);
            binLock.Unlock(); // The lock can be released now.
            
            if(activeGroup != bin->First()) {
                // Bring the group to the front of the bin.
                MakeGroupActive(bin, activeGroup);
            }

            // The group will automatically privatize all public locations.
            address = activeGroup->GetLocation();

#if defined(STEAL)
            SetAvailableForStealing<GS::GroupType>(context, allocInfo.Bin, 
                                                   activeGroup->CanBeStolen());
#endif
/* RET*/	if(address != nullptr) {
                return address;
            }
        }

#if defined(STEAL)
        // 4. Try to steal a location from a group in another bin. 
        // This reduces memory usage and fragmentation.
        address = TrySteal<Manager>(bin, context, allocInfo);
        if(address != nullptr) {
            return address;
        }
#endif

        // 5. A new group is needed.
        Statistics::GroupObtained(activeGroup);
        unsigned int locations = (GS::GroupSize - GS::HeaderSize) / allocInfo.Size;
        GS::BAType* manager = GS::GetBA(this, context->NumaNode);

        auto groupObject = manager->GetGroup<MemoryPolicy>(allocInfo.Size, locations, 
                                                           bin, context->ThreadId);
        activeGroup = static_cast<GS::GroupType*>(groupObject);

        if(activeGroup == nullptr) {
            return nullptr; // Failed to allocate memory!
        }

#if defined(STEAL)
        SetAvailableForStealing<GS::GroupType>(context, allocInfo.Bin, true);
#endif
        // Add the new group to the bin and return the requested location.
        AddNewGroup(bin, activeGroup);
        return activeGroup->GetLocation();
    }

    // Allocates a very large location (> 1MB) directly from the OS.
    void* AllocateFromOS(size_t size) {
        // Get the context associated with this thread.
        ThreadContext* context = GetCurrentContext();
        
        if(context == nullptr) {
            context = CreateContext();
        }

#if defined(PLATFORM_WINDOWS)
        // On Windows virtual memory is allocated on 64KB boundaries, 
        // so no extra work is needed.
        return memoryPolicy_.AllocateMemory(size, context->NumaNode);
#else
        size_t actualSize = size + Constants::SMALL_GROUP_SIZE;
        void* address = memoryPolicy_.AllocateMemory(actualSize, context->NumaNode);

        if(address == nullptr) {
            // Failed to allocate.
            return nullptr;
        }

        // Align the address.
        uintptr_t temp = (uintptr_t)address + Constants::SMALL_GROUP_SIZE - 1;
        OSHeader* header = reinterpret_cast<OSHeader*>(temp & ~(Constants::SMALL_GROUP_SIZE - 1));

        header->RealAddress = address;
        header->LocationAddress = (void*)((uintptr_t)header + sizeof(OSHeader));
        return header->LocationAddress;
#endif
    }

    // Returns a very large location (> 1MB) directly from to the OS.
    void DeallocateToOS(void* address) {
        // Get the context associated with this thread.
        ThreadContext* context = GetCurrentContext();

        if(context == nullptr) {
            context = CreateContext();
        }

#if defined(PLATFORM_WINDOWS)
        memoryPolicy_.DeallocateMemory(address, context->NumaNode);
#else
        OSHeader* header = reinterpret_cast<OSHeader*>((uintptr_t)address - sizeof(OSHeader));
        memoryPolicy_.DeallocateMemory(header->RealAddress, context->NumaNode);
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Determines if the specified group should be returned 
    // to the global list of unused groups.
    template <class Manager>
    bool IsGroupUnused(typename Selector<Manager>::GroupType* group, 
                       typename Selector<Manager>::BinType* bin) {
        // In order to be returned to the global list, the group 
        // needs to be completely empty and// at least 'ReturnAllowed'
        // allowed groups should remain in the bin.
        return group->IsFull() &&
               (bin->Count() > (bin->ReturnAllowed - 1));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Determines if the specified partially empty group should be 
    // returned to block allocator. Only groups with the size 
    // of a location multiple of the cache line can be returned.
    // All large groups can be returned.
    template <class Manager>
    bool IsGroupAlmostFull(typename Selector<Manager>::GroupType* group, 
                           typename Selector<Manager>::BinType* bin) {
        // In order to be returned to the global list, the group needs
        // to be completely empty and at least 'ReturnAllowed' allowed 
        // groups should remain in the bin.
        return Selector<Manager>::CanReturnPartial(bin) && 
               group->ShouldReturn() &&
               (bin->Count() > (bin->ReturnAllowed - 1));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a group that is only partially empty to the global lists.
    // This method presents some problems, like another thread adding 
    // the group to the list of public ones before the group could be 
    // marked as removed from the bin. See the comments in the code.
    template<class Manager>
    void ReturnPartiallyUsedGroup(typename Selector<Manager>::GroupType* group, 
                                  typename Selector<Manager>::BinType* bin, 
                                  ThreadContext* context) {
        Statistics::UsedGroupReturned(group);
        typedef typename Selector<Manager> GS; // Group selector.

        // Remove the group from the bin.
        bin->Remove(group);

#if defined(STEAL)
        // The group may be still referenced by bins that stole 
        // locations from it. This references need to be cleared 
        // before returning the group to the global list.
        RemoveStolenGroup<Manager>(context, group, bin->Number);
#endif

        // When we entered this method the group had no public locations.
        // If now it has, it means a foreign thread freed a location and added
        // the group to the public list. If it has, the group must be removed.
        // Synchronize access to the public list.
        SpinLock publicLock(&bin->PublicLock);

        if(group->HasPublic()) {
            Statistics::InvalidPublicGroup(group);

            if(bin->PublicGroup == group)	{
                bin->PublicGroup = static_cast<GS::GroupType*>(group->NextPublic);
            }
            else {
                // The group is not the first one in the list, we need to find it.
                auto previous = static_cast<GS::GroupType*>(bin->PublicGroup);
                auto current  = static_cast<GS::GroupType*>(previous->NextPublic);

                while(current != nullptr) {
                    if(current == group)	{
                        // Remove the group from the list.
                        previous->NextPublic = current->NextPublic;
                        break;
                    }

                    // Advance to next group.
                    previous = current;
                    current = static_cast<GS::GroupType*>(current->NextPublic);
                }
            }
        }

        // The lock can be removed.
        publicLock.Unlock();

        // Return the group to the block allocator.
        GS::BAType* manager = GS::GetBA(this, context->NumaNode);		
        manager->ReturnPartialGroup<MemoryPolicy>(group, GS::BAType::ADD_GROUP, 
                                                  bin->Number, context->ThreadId);

        // The last group can be removed only once. This prevents situations
        // when a group would be repeatedly linked and unlinked from the bin.
        if(bin->Count == (bin->ReturnAllowed - 1)) {
            bin->ReturnAllowed++;
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a group that is completely empty to the global list.
    // No synchronization is required because only the owner thread
    // can remove the group.
    template<class Manager>
    void ReturnUnusedGroup(typename Selector<Manager>::GroupType* group, 
                           typename Selector<Manager>::BinType* bin, 
                           ThreadContext* context) {
        Statistics::EmptyGroupReturned(group);
        typedef typename Selector<Manager> GS; // Group context.

        // The group is completely empty.
        group->ParentBin = nullptr;
        bin->Remove(group);

#if defined(STEAL)
        // The group may be still referenced by bins that stole 
        // locations from it. This references need to be cleared 
        // before returning the group to the global list.
        RemoveStolenGroup<Manager>(context, group, bin->Number);
#endif

        GS::BAType* manager = GS::GetBA(this, context->NumaNode);
        manager->ReturnFullGroup<MemoryPolicy>(group, true /* lock*/);

        // The last group can be removed only once. This prevents situations
        // when a group would be repeatedly linked and unlinked from the bin.
        if(bin->Count() == (bin->ReturnAllowed - 1)) {
            bin->ReturnAllowed++;
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a location from another thread that the one 
    // on which it was allocated. If this is the first public location 
    // from the group, the group is added to the list of public groups 
    // (managed by the owner bin).
    template <class Manager>
    void DeallocatePublic(void* address, typename Selector<Manager>::GroupType* group, 
                          typename Selector<Manager>::BinType* bin) {
        Statistics::PublicLocationFreed(group);

        unsigned int publicLocations = group->ReturnPublicLocation(address);
        
        if(publicLocations == 1) {
            // This is the first public location from the group 
            // and means that the group is not yet in the list of public ones.
            // Synchronize access to the public list.
            SpinLock publicLock(&bin->PublicLock);

            // It's possible that before we could acquire the lock
            // the parent thread of the group returned it to the list
            // of partial groups. The group may be still there, or another
            // thread could have took it. We can add the group to the public 
            // list if the group is owned, and the owner hasn't changed.
            if(group->ParentBin == bin) {
                group->NextPublic = bin->PublicGroup;
                bin->PublicGroup = group;
            }
         }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Deallocates the specified location. Handles both owner and foreign threads.
    template <class Manager>
    void Deallocate(void* address, typename Selector<Manager>::GroupType* group) {
        typedef typename Selector<Manager> GS; // Group context.
        GS::BinType* bin = reinterpret_cast<GS::BinType*>(group->ParentBin);

        if(*(volatile uintptr_t*)&group->ParentBin != 0) {
            // The group is owned by a thread, get the associated context.
            ThreadContext* context = GetCurrentContext();

            if(group->ThreadId == context->ThreadId) {
                // The group belongs to the current thread. 
                // If the group is completely free (and it's allowed), 
                // we return it to the global pool of free groups.
                group->ReturnPrivateLocation(address);

                if(IsGroupUnused<Manager>(group, bin)) {
                    ReturnUnusedGroup<Manager>(group, bin, context);				
                    return;
                }
                else if(group != bin->First()) { 
                    // We don't touch the active group if it's not empty.
                    // There are at least 2 groups in the bin.
                    if(group != bin->First()->Next) {
                        // Bring the group to the second position (the first position
                        // is always used by the active group). This guarantees that 
                        // if the second group has no free locations, all the other 
                        // ones don't have too (and also improves cache locality).
                        Statistics::BroughtToFront();

                        bin->Remove(group);
                        bin->AddAfter(bin->First(), group);
                        return;
                    }
                }
            } // END: group->ThreadId == context->ThreadId
            else {
                // This thread is not the owner of the group. 
                // The location is added to the synchronized public list.
                DeallocatePublic<Manager>(address, group, bin);
                return;
            }
        } // END: group->IsOwnedn
        else {
            // If the group doesn't belong to a thread, the only way 
            // to free a location is by adding it to the public list. 
            // The public list must be made private for 'IsEmpty' to work.
            unsigned int publicLocations = group->ReturnPublicLocation(address);

            if(group->MayBeFull(publicLocations)) {
                // The group is (probably) full and we must try to remove 
                // it from the partial list and add it to the full list 
                // (it's the block allocator's responsibility to check 
                // that the group is still in the partial list).
                ThreadContext* context = GetCurrentContext();
                auto manager = Selector<Manager>::GetBA(this, context->NumaNode);

                manager->ReturnPartialGroup<MemoryPolicy>(group, GS::BAType::REMOVE_GROUP, 
                                                          bin->Number, context->ThreadId);
            }
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    HugeLocation* HugeFromClient(void* address) {
        return reinterpret_cast<HugeLocation*>((void*)(((uintptr_t)address) - 
                                               Constants::HUGE_HEADER_SIZE));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* HugeToClient(void* address) {
        return (void*)(((uintptr_t)address) + Constants::HUGE_HEADER_SIZE);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to remove the specified huge location.
    void RemoveHugeLocation(HugeLocation* location, ThreadContext* context) {
        // Under Windows there are 3 situations:
        // 1. The location has no parent.
        // 2. The location has a parent with linked locations.
        // 3. The location has a parent with linked locations and/or a block header.
#if defined(PLATFORM_WINDOWS)
        if(location->Parent == nullptr) {
            // No other locations are linked with this one.
            memoryPolicy_.DeallocateMemory(location->Address, context->NumaNode);
            return;
        }
        
        HugeLocation* parent = reinterpret_cast<HugeLocation*>(location->Parent);

        // Decrement the used location counter.
        if(parent->Release()) { 
            // This was the last location in the series.
            if(parent->HasBlock) {
                // The location had an associated block header.
                auto manager = Selector<SmallBAType>::GetBA(this, context->NumaNode);
                manager->RemoveBlock<MemoryPolicy>(parent->Block);
            }
            else {
                memoryPolicy_.DeallocateMemory(parent->Address, context->NumaNode);
            }
        }
#else
        memoryPolicy_.DeallocateMemory(location->Address, context->NumaNode);
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Walks the huge location bin array and removes locations
    // that have not been used recently. Called by a thread that runs 
    // in the background with a very low priority.
    void CleanHugeCache() {
        unsigned int currentTime = ThreadUtils::GetSystemTime();
        ThreadContext* context = GetCurrentContext();

        for(unsigned int candidate = Constants::HUGE_START; 
            candidate < Constants::HUGE_BINS; candidate++) {
            // Does the bin have any locations?
            //unsigned int lastTime = hugeBins_[candidate].Cache.GetOldestTime();
            //unsigned int timeDiff = currentTime - lastTime;
            //unsigned int count = hugeBins_[candidate].Cache.GetCount();

            //if((count > 0) && (timeDiff > hugeBins_[candidate].CacheTime)) {
            //    // This bin has some old and unused locations; 
            //    // half of them will be removed. Remove at least one location.
            //    unsigned int count = std::max(1, count / 2);

            //    for(unsigned int j = 0; j < count; j++) {
            //        HugeLocation* location = hugeBins_[candidate].Cache.Pop();
            //        
            //        if(location == nullptr) {
            //            break; // No more locations in the stack.
            //        }

            //        RemoveHugeLocation(location, context);
            //    }

            //    hugeBins_[candidate].DecreaseCacheSize();
            //}
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Creates the thread that cleans the huge location cache on a regular interval.
    void CreateCacheCleaningThread() {
        bool state = Memory::ReadValue(&cacheThreadInitialized_);

        if(!state) {
            // Acquire the lock. Will be automatically released by the destructor.
            SpinLock lock(&cacheThreadLock_);

            if(!cacheThreadInitialized_) {
                // Create the thread.
                void* data = Memory::Allocate(sizeof(CacheThreadArgs));
                CacheThreadArgs* cacheArgs = reinterpret_cast<CacheThreadArgs*>(data);

                if(cacheArgs == nullptr) {
                    return; // Not enough memory available!
                }

                cacheArgs->Allocator = this;
                cacheArgs->Timeout = Constants::CACHE_CLEANING_INTERVAL;
                cacheArgs->ThreadHandle = ThreadUtils::CreateThread(Allocator::CacheCleaningThread,
                                                                    cacheArgs);

                Memory::WriteValue(&cacheThreadInitialized_, true);
            }
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Makes sure that the huge cache cleaning thread is started.
    // Called only by the huge location allocation method.
    void EnsureCacheThreadActive() {
        if(!cacheThreadInitialized_) {
            CreateCacheCleaningThread();
        }
    }
 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // The thread that cleans the huge location cache at regular intervals.
    static void CacheCleaningThread(void* args) {
#if defined(PLATFORM_WINDOWS) && defined(_DEBUG)
        ThreadUtils::SetThreadName(ThreadUtils::GetCurrentThreadId(), 
                                    Constants::CACHE_THREAD_NAME);
#endif

        CacheThreadArgs* threadArgs = reinterpret_cast<CacheThreadArgs*>(args);
        ThreadUtils::SetThreadLowPriority(threadArgs->ThreadHandle);

        // The thread never exits.
        while(true) {
            ThreadUtils::Sleep(threadArgs->Timeout);
            threadArgs->Allocator->CleanHugeCache();
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Initializes a huge location that has no other linked locations.
    void InitializeHugeLocation(void* address, unsigned int bin, unsigned int size) {
        HugeLocation* location = reinterpret_cast<HugeLocation*>(address);
        location->Address = address;
        location->Bin =& hugeBins_[bin];
        location->Size = size;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Initializes a huge location that has linked locations and/or block headers (small groups).
    // Used only under Windows.
    void InitializeHugeLocationEx(void* address, unsigned int bin, unsigned int size, 
                                  bool hasBlock, void* parent, void* block) {
        HugeLocation* location = reinterpret_cast<HugeLocation*>(address);
        location->Address = address;
        location->Bin =& hugeBins_[bin];
        location->Size = size;
        location->HasBlock = hasBlock;
        location->Block = block;
        location->Parent = parent;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to create a block header with small groups in the unused space of the
    // specified huge location. If the space could be used, it initializes the parent location.
    // Used only under Windows.
    bool UnusedAsGroups(void* address, char* start, char* end, unsigned int bin, 
                        unsigned int size, bool addRef, ThreadContext* context) {
        unsigned int available = (uintptr_t)end - (uintptr_t)start;

        if(available >= Constants::SMALL_GROUP_SIZE) {
            HugeLocation* parent = reinterpret_cast<HugeLocation*>(address);
            if(addRef) parent->AddRef();

            // There is space for at least one small group.
            // Compute the bitmap of the available groups.
            unsigned int nGroups = available / Constants::SMALL_GROUP_SIZE;
            unsigned __int64 bitmap = (1 << nGroups) - 1;

            auto manager = Selector<SmallBAType>::GetBA(this, context->NumaNode);
            auto block = manager->AddBlock<MemoryPolicy>(address, bitmap,
                                                         nGroups, address);

            InitializeHugeLocationEx(address, bin, size, true, parent, block);
            return true;
        }

        return false;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to create huge locations in the unused space of the 
    // specified huge location. The created location have the size 
    // of the parent. If the corresponding cache is full,  we try to create 
    // small groups in the remaining space. If the space could be used, 
    // it initializes the parent location. Only under Windows.
    bool UnusedAsCache(void* address, char* start, char* end, 
                       unsigned int bin, unsigned int size, 
                       ThreadContext* context) {
        bool asGroup = false;
        bool foundAvailable = false; // The remaining space could be used.
        HugeLocation* parent = reinterpret_cast<HugeLocation*>(address);

        while(start < end) {
            // Align to the size of a small group.
            start = (char*)(((uintptr_t)start + Constants::SMALL_GROUP_SIZE - 1) & 
                            ~(Constants::SMALL_GROUP_SIZE - 1));

            if(start >= end) {
                break; // We are past the allocated block.
            }
            
            if(!foundAvailable) {
                foundAvailable = true;
                parent->AddRef();
            }

            // We try to add to the cache first.
            HugeLocation* temp = reinterpret_cast<HugeLocation*>(start);
            InitializeHugeLocationEx(temp, bin, size, false, parent, nullptr);
            parent->AddRef(); // Optimistically increase the reference count.
            
            //temp = hugeBins_[bin].Cache.Push(temp);

            //if(temp != nullptr) {
            //    // The cache is full, treat the unused memory as small groups.
            //    parent->Release(); // The counter needs to be decremented.
            //    asGroup = UnusedAsGroups(address, start, end, bin, size,
            //                             false /*addRef*/, context);
            //    break;
            //}

            start += size; // Advance to next position.
        }

        if(foundAvailable && !asGroup) {
            // No groups have been created, the parent needs to be initialized_.
            InitializeHugeLocationEx(address, bin, size, false, parent, nullptr);
        }

        return foundAvailable;
    }
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Allocates a huge location.
    void* AllocateHuge(unsigned int size) {
        // The object is large and must be allocated from the OS 
        // 1. Check if an unused locations with the corresponding size is cached.
        // 2. If not, search the next 2 bins for a cached location and take it from there.
        // 3. If a cached location could not be found, allocate directly from the OS.
        EnsureCacheThreadActive();
        ThreadContext* context = GetCurrentContext();

        if(context == nullptr) {
            context = CreateContext();
        }

        size += Constants::HUGE_HEADER_SIZE;
        unsigned int startBin = size / Constants::HUGE_GRANULARITY;
        
        if((size % Constants::HUGE_GRANULARITY) > 0) {
            startBin++;
        }

        void* address = nullptr;
        /*void* address = hugeBins_[startBin].Cache.Pop();

        if(address != nullptr) {
            return HugeToClient(address);
        }
*/
        // The demand for this size may be very high, 
        // try to increase the cache.
        hugeBins_[startBin].IncreaseCacheSize();
        
        // No location with a size near to the specified one could be found.
        // Allocate from the OS (aligned to 16KB, else small groups wouldn't work).
#if defined(PLATFORM_WINDOWS)
        // On Windows allocations are performed on a 64KB boundary and the location
        // is always properly aligned. We round up the size to the nearest
        // 64KB multiple so that no memory is wasted. The memory that remains 
        // is used for other locations:
        // 1. If the allocation size is <= 32KB, the unused memory is added to the cache.
        // 2. For larger sizes, the unused memory is split into up to 3 small groups.
        unsigned int objSize = size;
        objSize = (objSize + Constants::HUGE_GRANULARITY - 1) & 
                   ~(Constants::HUGE_GRANULARITY - 1);
        size = (size + Constants::WINDOWS_GRANULARITY - 1) &
                ~(Constants::WINDOWS_GRANULARITY - 1);

        address = memoryPolicy_.AllocateMemory(size, context->NumaNode);
        
        // Set the limits of the unused memory.
        char* unusedP = (char*)address + objSize;
        char* endP = (char*)address + size;
        bool foundAvailable = false;

        if(objSize <= Constants::HUGE_SPLIT_POSITION) {
            foundAvailable = UnusedAsCache(address, unusedP, endP, 
                                           startBin, objSize, context);
        }
        else {
            // Align to the size of a small group.
            unusedP = (char*)(((uintptr_t)unusedP + Constants::SMALL_GROUP_SIZE - 1) & 
                             ~(Constants::SMALL_GROUP_SIZE - 1));
            foundAvailable = UnusedAsGroups(address, unusedP, endP, startBin, 
                                            objSize, true /*addRef*/, context);
        }

        if(!foundAvailable) {
            // The memory could not be used.
            HugeLocation* parent = reinterpret_cast<HugeLocation*>(address);
            InitializeHugeLocation(address, startBin, size);
        }
#else
        size = (size + Constants::HUGE_GRANULARITY - 1) & 
                ~(Constants::HUGE_GRANULARITY - 1);
        address = memoryPolicy_.AllocateMemory(size, context->NumaNode);

        InitializeHugeLocation(address, startBin, size);
#endif
        return HugeToClient(address);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Deallocates a huge location.
    // Under Windows, the location is deallocated only if the reference 
    // count of the parent reaches 0.
    void DeallocateHuge(void* address) {
        HugeLocation* location = HugeFromClient(address);
        HugeBin* bin = location->Bin;

        // Try to add the location to the cache.
        //location = bin->Cache.Push(location);

        //if(location != nullptr) {
        //    // The cache is full and the location is no longer needed.
        //    ThreadContext* context = GetCurrentContext();
        //    RemoveHugeLocation(location, context);
        //}
    }


public:
    Allocator() {
        initialized_ = false;
        initLock_ = 0;
        cacheThreadInitialized_ = false;
        cacheThreadLock_ = 0;
        threadContextPool_ = ObjectPool(Constants::THREAD_CONTEXT_ALLOCATION_SIZE, 
                                        Constants::THREAD_CONTEXT_SIZE,
                                        Constants::THREAD_CONTEXT_CACHE);

        blockAllocatorPool_ = ObjectPool(Constants::BA_ALLOCATION_SIZE,
                                         Constants::BA_SIZE,
                                         Constants::BA_CACHE);
    
        // Initialize the memory policy and the block allocators.
        memoryPolicy_.Initialize();
        unsigned int lastNode = memoryPolicy_.GetNodeNumber() +
                                (memoryPolicy_.IsNuma() ? 0 : 1);

        for(unsigned int node = 0; node < lastNode; node++) {
            smallBlockAlloc_[node] = reinterpret_cast<SmallBAType*>(blockAllocatorPool_.GetObject());
            largeBlockAlloc_[node] = reinterpret_cast<LargeBAType*>(blockAllocatorPool_.GetObject());

            new(smallBlockAlloc_[node]) SmallBAType();
            new(largeBlockAlloc_[node]) LargeBAType();

            smallBlockAlloc_[node]->Initialize<MemoryPolicy>(&memoryPolicy_, node);
            largeBlockAlloc_[node]->Initialize<MemoryPolicy>(&memoryPolicy_, node);
        }

        // Initialize the huge bins.
        for(unsigned int i = Constants::HUGE_START; i < Constants::HUGE_BINS; i++) {
            hugeBins_[i].CacheSize = Constants::HugeCacheSize[i];
            hugeBins_[i].CacheTime = Constants::HugeCacheTime[i];
            hugeBins_[i].MaxCacheSize = hugeBins_[i].CacheSize;
            hugeBins_[i].ExtendedCacheSize = hugeBins_[i].MaxCacheSize*  8;
            //hugeBins_[i].Cache.SetMaxObjects(Constants::HugeCacheSize[i]);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Allocates a location having the specified size.
    void* Allocate(size_t size) {
        // Determine in which category (small, large, huge) the allocation 
        // size is, and allocate using the corresponding method.
        if(size <= Constants::MAX_SMALL_SIZE) {
            return Allocate<SmallBAType>(size);	
        }
        else if(size <= Constants::MAX_LARGE_SIZE) {
            return Allocate<LargeBAType>(size);	
        }
        else if(size <= Constants::MAX_HUGE_SIZE) {
            return AllocateHuge(size);
        }

        // The location can't be handled by the allocator
        // and the request is forwarded  directly to the OS. 
        // The location must be aligned to the size of a small group.
        return AllocateFromOS(size);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Deallocates the location indicated by the specified address.
    void Deallocate(void* address) {
        // Do nothing if the address is nullptr (all allocators behave like this).
        if(address == nullptr) {
            return;
        }

        // Determine the group to which this location belongs.
        void* alignedAddress = (void*)((uintptr_t)address &  
                                ~((uintptr_t)Constants::SMALL_GROUP_SIZE - 1));

        if(!IsHugeLocation(address, alignedAddress)) {
            if(!IsLargeLocation(address, alignedAddress)) {
                // The location is "small". We mask the first 
                // log2(SMALL_GROUP_SIZE) bits to obtain the group address.
                Group* group = reinterpret_cast<Group*>(alignedAddress);
                Deallocate<SmallBAType>(address, group);
            }
            else {
                // The location is "large". See if it's located in the first subgroup. 
                // If not, the start address of the group needs to be recomputed.
                LargeGroup* group = reinterpret_cast<LargeGroup*>(alignedAddress);
                auto castedGroup = reinterpret_cast<LargeTraits::NodeType*>(group);
                unsigned int subgroup = LargeTraits::PolicyType::GetSubgroup(castedGroup);

                unsigned int subgroupOffset = (subgroup * Constants::SMALL_GROUP_SIZE);
                group = reinterpret_cast<LargeGroup*>((uintptr_t)group - subgroupOffset);
                Deallocate<LargeBAType>(address, group);
            }
        }
        else {
            if(!IsOSLocation(address, alignedAddress)) {
                // The location is "huge".
                DeallocateHuge(address);
            }
            else {
                // The location should be returned directly to the OS.
                DeallocateToOS(address);
            }
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* Realloc(void* address, size_t newSize) {
        //! TODO: Not yet implemented (problems on 64 bit systems with the assembly code).
        return nullptr;
    }
};

} // namespace Base
#endif
