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
// Implements the allocator for large memory blocks.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_BLOCK_MANAGER_HPP
#define PC_BASE_ALLOCATOR_BLOCK_MANAGER_HPP

#include "SpinLock.hpp"
#include "ObjectPool.hpp"
#include "Memory.hpp"
#include "AllocatorConstants.hpp"
#include "Bitmap.hpp"
#include "FreeObjectList.hpp"
#include "Statistics.hpp"
#include "Atomic.hpp"
#include "HugeLocation.hpp"

#if defined(PLATFORM_WINDOWS)
    #include <Windows.h>
#else
    static_assert(false, "Not yet implemented.");
#endif


// Small and medium groups are allocated in blocks (1MB) that contain a maximum
// of 64 groups (limited by the 64-bit bitmap used to keep track of used blocks).
// Keeping track of the groups makes it possible to return the memory to the system
// when it's no longer needed. A specified number of blocks are cached to prevent
// situations in which a group is repeatedly obtained and returned to the OS.
template <unsigned int BinNumber, unsigned int BlockSize, unsigned int GroupSize, 
          unsigned int CacheSize, class GroupType, class BinType, class PartialTraits>
class BlockAllocator {
private:
    // Nested types
    #pragma pack(push)
    #pragma pack(1)
    struct BlockDescriptor : public ObjectList<>::Node {
        // Full block  - all groups are unused, available.
        // Empty block - all groups are used, unavailable.
        void* StartAddress;           // The address of the first usable group.
        void* RealAddress;            // The address of the first byte of the block.
        unsigned __int64 GroupBitmap; // Keeps track of used groups.
        unsigned __int64 FullBitmap;  // The bitmap when the block is full (no group is used).
        HugeLocation* HugeParent;     // The associated huge location (only under Windows).
        unsigned int FreeGroups;      // The number of free groups in the block.
        unsigned int NumaNode;        // Only for NUMA.

        // Padded to cache line by the object pool.
    };
    #pragma pack(pop)

    // Helper that creates a bitmap that describes a block when it's unused.
    // (sets the first BSize/GSize bits to 1).
    template <unsigned int BSize, unsigned int GSize>
    struct EmptyMask {
        enum { N = BSize / GSize };
        static const unsigned __int64 Mask = EmptyMaskImpl<N>::Mask;
    };

    template <unsigned int N>
    struct EmptyMaskImpl {
        static const unsigned __int64 Mask = (1 << N) - 1;
    };

    template <>
    struct EmptyMaskImpl<64> {
        // Special case for N = 64 (shifting undefined).
        static const unsigned __int64 Mask = -1;
    };

    typedef typename EmptyMask<BlockSize, GroupSize> Empty;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    ObjectPool blockDescriptorPool_; // 1 cache line.
    ObjectList<> fullBlockList_;
    ObjectList<> emptyBlockList_;
    void* allocator_;
    unsigned int lock_;
    unsigned int numaNode_;

    // The bins that contain partial freed groups.
    ObjectList<typename PartialTraits::NodeType,
               typename PartialTraits::PolicyType> partialFreeGroups_[BinNumber];

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Allocates and initializes a block of memory.
    template <class MemoryPolicy>
    BlockDescriptor* AllocateBlock() {
        // All groups need to be aligned to the size of the group.
        // As a result, we need to allocate more memory that actually 
        // required to guarantee that the groups are properly aligned.
        MemoryPolicy* memPolicy = static_cast<MemoryPolicy*>(allocator_);
        void* rawBlockAddr;
        void* alignedBlockAddr;

#if defined(PLATFORM_WINDOWS)
        // On Windows (all versions) allocation is performed on a 64 KB boundary,
        // so the block is always properly aligned.
        rawBlockAddr = memPolicy->AllocateMemory(BlockSize, numaNode_);
        alignedBlockAddr = rawBlockAddr;
#else
        rawBlockAddr = memPolicy->AllocateMemory(BlockSize + GroupSize, numaNode_);
        alignedBlockAddr = (void*)(((uintptr_t)rawBlockAddr + GroupSize - 1) & 
                                  ~(GroupSize - 1));
#endif	
        // Get a block descriptor from the pool.
        auto block = reinterpret_cast<BlockDescriptor*>(blockDescriptorPool_.GetObject());             

        if((rawBlockAddr != nullptr) && (block != nullptr)) {
            // Initialize the block header.
            block->GroupBitmap = block->FullBitmap = Empty::Mask;
            block->RealAddress = rawBlockAddr;
            block->StartAddress = alignedBlockAddr;

#if defined(PLATFORM_NUMA)
            block->numaNode_ = numaNode_;
#endif
            return block;
        }
        else {
            // No memory could be allocated.
            if(rawBlockAddr != nullptr) {
                memPolicy->DeallocateMemory(rawBlockAddr, numaNode_);
            }

            if(block != nullptr) {
                blockDescriptorPool_.ReturnObject(block);
            }
        }

        return nullptr;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Deallocates the specified block of memory.
    template <class MemoryPolicy>
    void DeallocateBlock(BlockDescriptor* block)	{
        // Deallocate the associated memory 
        // and return the descriptor to the object pool.
        static_cast<MemoryPolicy*>(allocator_)
            ->DeallocateMemory(block->RealAddress, numaNode_);
        blockDescriptorPool_.ReturnObject(block);
    }

    // Gets the first available group from the specified block.
    GroupType* GetGroupFromBlock(BlockDescriptor* block, unsigned int& isEmpty) {
        // Find the first available group.
        // The found index is guaranteed to be valid because:
        // 1. Only this thread can get groups from the block (the access is serialized).
        // 2. Blocks that return groups only set bits, don't reset them.
        unsigned int groupIndex = Bitmap::SearchForward(block->GroupBitmap);
        unsigned __int64* bitmapAddr = (unsigned __int64*)&block->GroupBitmap;
        unsigned __int64 oldBitmap = Atomic::ResetBit64(bitmapAddr, groupIndex);

        // If only one bit was set, the block has no groups anymore
        // and must be removed from the "full" list and added to the "empty" list.
        isEmpty = ((oldBitmap & (oldBitmap - 1)) == 0); // Resets the first set bit.
    
        // Initialize the group.
        void* groupAddr = (char*)block->StartAddress + (groupIndex*  GroupSize);
        GroupType* group = reinterpret_cast<GroupType*>(groupAddr);
        group->ParentBlock = block;

#if defined(PLATFORM_WINDOWS)
        if(block->HugeParent != nullptr) {
            block->HugeParent->AddRef();			
        }
#endif
        return group;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the specified group to the owner block.
    unsigned int ReturnGroupToBlock(BlockDescriptor* block, GroupType* group) {
        // Mark the group as unused.
        unsigned int groupIndex = ((char*)group - (char*)block->StartAddress) / GroupSize;
        unsigned __int64* bitmapAddr = (unsigned __int64*)&block->GroupBitmap;
        unsigned __int64 oldBitmap = Atomic::SetBit64(bitmapAddr, groupIndex);

        // If the block had no free groups it must be removed 
        // from the empty list and added to the full list.
        if(oldBitmap == 0) {
            // The block was empty.
            return BLOCK_WAS_EMPTY;
        }
        else if((oldBitmap | (oldBitmap + 1)) == block->FullBitmap) {
            // Sets the lowest unset bit and checks if all bits are set.
            // The block is now completely full (no group is used).
            return BLOCK_IS_FULL;
        }

#if defined(PLATFORM_WINDOWS)
        if((block->HugeParent != nullptr) && block->HugeParent->Release()) {
            return BLOCK_FROM_HUGE;
        }
#endif
        return BLOCK_NO_ACTION;
    }

public:
    typedef typename GroupType GroupT;
    typedef typename BlockAllocator<BinNumber, BlockSize, GroupSize, 
                                    CacheSize, GroupType, BinType, PartialTraits> BAType;
    typedef typename ObjectList<typename PartialTraits::NodeType,
                                typename PartialTraits::PolicyType> PartialListType;

    static const unsigned int REMOVE_GROUP    = 1;
    static const unsigned int ADD_GROUP       = 2;

    static const unsigned int BLOCK_NO_ACTION = 0;
    static const unsigned int BLOCK_FROM_HUGE = 1;
    static const unsigned int BLOCK_WAS_EMPTY = 2;
    static const unsigned int BLOCK_IS_FULL   = 3;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class MemoryPolicy>
    void Initialize(void* allocator, unsigned int numaNode) {
        lock_ = 0;
        allocator_ = allocator;
        numaNode_ = numaNode;
        auto memoryPolicy = static_cast<MemoryPolicy*>(allocator_);

        memoryPolicy->SetBlockAllocator<BAType>(this, numaNode_);
        memoryPolicy->BlockUnavailable<BAType>(numaNode_);

        blockDescriptorPool_ = ObjectPool(Constants::BLOCK_DESCRIPTOR_ALLOCATION_SIZE, 
                                          Constants::BLOCK_DESCRIPTOR_SIZE,
                                          Constants::BLOCK_DESCRIPTOR_CACHE);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets a group. If no group is available, the group 
    // is allocated from a new memory block.
    // 1. Check if a group that is not completely empty 
    //    is available in the specified bin. If a group is found, 
    //    remove it from the bin and return it.
    // 2. If the first block doesn't exist or it doesn't have 
    //    any unused group, allocate a new block.
    // 3. Allocate from the first block (guaranteed to have 
    //    at least an unused group by step 2).
    template <class MemoryPolicy>
    GroupType* GetGroup(unsigned int locationSize, unsigned int locations, 
                        BinType* bin, unsigned int currentThreadId) {
        // Will be released when the method exists.
        SpinLock managerLock(&lock_); 
        unsigned int isEmpty = 0;

        // Try to get the group from the list of partially used groups.
        // If it fails, get a unused group.
        auto groupObject = partialFreeGroups_[bin->Number].RemoveFirst();
        auto group = reinterpret_cast<GroupType*>(groupObject);

        if(group != nullptr) {
            // We could get a group from the partial list; mark it as owned.
            group->InitializeUsed(currentThreadId);
            Memory::WriteValue((uintptr_t*)&group->ParentBin, (uintptr_t)bin);
            return group;
        }

        // The partial list had no group available, try to get one from the full list.
        if(fullBlockList_.Count > 0) {
            // Get a group from the first block with unused groups.
            auto descriptor = static_cast<BlockDescriptor*>(fullBlockList_.First);
            group = GetGroupFromBlock(descriptor, isEmpty);
        
            if(isEmpty) {
                // The block has no free groups anymore. It needs to be moved
                // from the list of (partially)full blocks to the one with empty blocks.
                emptyBlockList_.AddFirst(fullBlockList_.RemoveFirst());
            }

            // Initialize the unused group.
            group->InitializeUnused(locationSize, locations, currentThreadId);
            Memory::WriteValue((uintptr_t*)&group->ParentBin, (uintptr_t)bin);
            return group;
        }
        else {
            // This block allocator has no group available.
            // Try to get a group from another NUMA node first.
            MemoryPolicy* memPolicy = static_cast<MemoryPolicy*>(allocator_);

            // Announce that there are no groups available anymore.
            memPolicy->BlockUnavailable<BAType>(numaNode_);
            groupObject = memPolicy->GetGroup<BAType>(numaNode_, currentThreadId);
            group = reinterpret_cast<GroupType*>(groupObject);
            
            if(group != nullptr) {
                group->InitializeUnused(locationSize, locations, currentThreadId);
                Memory::WriteValue((uintptr_t*)&group->ParentBin, (uintptr_t)bin);
                return group;
            }

            // A new block needs to be allocated.
            BlockDescriptor* block = AllocateBlock<MemoryPolicy>();

            if(block == nullptr) {
                return nullptr; // Failed to allocate block.
            }

            fullBlockList_.AddFirst(block);

            // Announce that there are groups available now.
            memPolicy->BlockAvailable<BAType>(numaNode_);

            // Get a group from the newly allocated block and initialize it.
            group = GetGroupFromBlock(static_cast<BlockDescriptor*>(block), isEmpty);
            group->InitializeUnused(locationSize, locations, currentThreadId);
            Memory::WriteValue((uintptr_t*)&group->ParentBin, (uintptr_t)bin);

            // The block cannot be empty from the first allocation
            // with the current values (64 groups/block).
            return group;
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to get an unused group without allocating a new block.
    // Used only by NUMA systems.
    template <class MemoryPolicy>
    GroupType* TryGetGroup(unsigned int currentThreadId) {
        // Will be released when the method exists.
        SpinLock managerLock(&lock_);

        if(fullBlockList_.Count > 0) {
            unsigned int isEmpty = false;
            auto descriptor = static_cast<BlockDescriptor*>(fullBlockList_.First);
            auto group = GetGroupFromBlock(descriptor, isEmpty);
            
            group->ThreadId = currentThreadId;

            if(isEmpty) {
                // The block has no free groups anymore.
                emptyBlockList_.AddFirst(fullBlockList_.RemoveFirst());
            }

            return group;
        }

        return nullptr; // No unused group found.
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the specified group to it's parent block.
    template <class MemoryPolicy>
    void ReturnFullGroup(GroupType* group, bool takeLock) {
        auto block = reinterpret_cast<BlockDescriptor*>(group->ParentBlock);

#if defined(PLATFORM_NUMA)
        if(block->numaNode_ == numaNode_) {
#endif
            // Return the group to it's parent block (the operation is atomic).
            // If necessary, move the block from the empty list to the full list.
            unsigned int result = ReturnGroupToBlock(block, group);

            if(result == BLOCK_NO_ACTION) {
                // Main case (> 95%).
                return;
            }
            else if(result >= BLOCK_WAS_EMPTY) {
                // Will be released when the method exists.
                SpinLock managerLock(&lock_);

                if(result == BLOCK_IS_FULL) {
                    // The block is full; check if it should be kept into cache.
                    // It can be returned to the OS only if no other thread took
                    // a group before we acquired the manager lock and only if 
                    // enough blocks remain in the cache.
                    if((block->GroupBitmap == Empty::Mask) &&
                       ((fullBlockList_.Count + emptyBlockList_.Count) > CacheSize)) {
                        // Return the block to the OS.
                        fullBlockList_.Remove(block);
                        DeallocateBlock<MemoryPolicy>(block);
                    }
                }
                else if (block->GroupBitmap != 0) { 
                    // The block hasn't been changed by another thread.
                    // The bitmap becomes 0 only if a thread took a group
                    // from the block. Threads that return groups only set bits, 
                    // so the bitmap cannot be 0 in this case.
                    emptyBlockList_.Remove(block);
                    fullBlockList_.AddFirst(block);
                }
            }
#if defined(PLATFORM_WINDOWS)
            else if(result == BLOCK_FROM_HUGE) {
                // The groups is part of a huge location that is no longer referenced.
                // Will be released when the method exists.
                SpinLock managerLock(&lock_);

                // We are the ones who need to deallocate the huge location.
                // The start of the location is the 'RealAddress' member 
                // of the block, so calling 'DeallocateBlock' will return 
                // to the OS the whole huge location.
                fullBlockList_.Remove(block);
                DeallocateBlock<MemoryPolicy>(block);
            }
#endif

#if defined(PLATFORM_NUMA)
        } // END: block->numaNode_ == numaNode_
        else {
            // The block belongs to another NUMA node (it was taken from there).
            MemoryPolicy* memPolicy = static_cast<MemoryPolicy*>(allocator_);
            memPolicy->ReturnGroup<BAType>(group, block->numaNode_);
        }
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Adds/removes the specified group to/from the associated partial list.
    template <class MemoryPolicy>
    void ReturnPartialGroup(GroupType* group, unsigned int action, 
                            unsigned int bin, unsigned int currentThreadId) {
        // Partially used groups are not returned to the parent 
        // NUMA node until they are completely unused. This prevents 
        // nodes to access locations that reside on another nodes.
        auto block = reinterpret_cast<BlockDescriptor*>(group->ParentBlock);
        auto partialList = &partialFreeGroups_[bin];
        SpinLock managerLock(&lock_); // Will be released when the method exists.

        if(action == ADD_GROUP)	{
            // This group is partially free, try to return it to one 
            // of the free lists. It's possible that another thread 
            // added the group to the full list or to the partial list
            // until we reached this point. In this case, the group should 
            // not be added again. Another possibility is that after 
            // the group  was added to one of the lists, a thread took 
            // the group and is using it now (this happens if the thread ID 
            // of the group doesn't match the ID of the current thread). 
            // Again, the group should not be added to the partial list.
            if(group->ThreadId != currentThreadId) {
                return;
            }

            group->ParentBin = nullptr;
            partialList->AddFirst(group);
        }
        else {
            // The group needs to be removed from the partial list
            // and added to the full list.
            if(group->ParentBin != nullptr) {
                // The group isn't in the partial list anymore. 
                // This means that it should not be added to the full list 
                // too, because it may appear more than once in the bins.
                return;
            }

            partialList->Remove(group);
            managerLock.Unlock();
            ReturnFullGroup<MemoryPolicy>(group, false /* lock already taken */);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Creates a block descriptor for the specified memory range.
    template <class MemoryPolicy>
    void* AddBlock(void* address, unsigned __int64 bitmap, 
                   unsigned int groups, void* parent) {
        auto block = reinterpret_cast<BlockDescriptor*>(blockDescriptorPool_.GetObject());             

        // Initialize the block header.
        block->Next = block->Previous = nullptr;
        block->GroupBitmap = block->FullBitmap = bitmap;
        block->FreeGroups = groups;
        block->RealAddress = address;
        block->StartAddress = address;
        block->HugeParent = reinterpret_cast<HugeLocation*>(parent);

        // Will be automatically released by the destructor.
        SpinLock managerLock(&lock_);
        fullBlockList_.AddFirst(block);
        return block;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Removes the specified block descriptor.
    template <class MemoryPolicy>
    void RemoveBlock(void* address) {
        // Will be automatically released by the destructor.
        SpinLock managerLock(&lock_); 

        BlockDescriptor* block = reinterpret_cast<BlockDescriptor*>(address);
        fullBlockList_.Remove(block);
        DeallocateBlock<MemoryPolicy>(block);
    }

    // For debugging only.
    unsigned int GetEmptyCount() { 
        return emptyBlockList_.Count; 
    }

    unsigned int GetFullCount() { 
        return fullBlockList_.Count; 
    }
};

#endif
