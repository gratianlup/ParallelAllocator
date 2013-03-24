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
// Implements the group used for large locations.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef LARGE_GROUP_H
#define LARGE_GROUP_H

#include "ObjectList.hpp"
#include "AllocatorConstants.hpp"
#include "UnrolledLoops.hpp"
#include "Bitmap.hpp"

#if defined(PLATFORM_64)
    #if defined PLATFORM_WINDOWS
        #include <intrin.h>
    #else
        static_assert(false, "Not yet implemented.");
    #endif
#endif

namespace Base {

// Holds the head of the list with public locations.
struct BitmapHolder {
    static const BitmapHolder None;

#if defined(PLATFORM_32)
    unsigned int Bitmap : 20;
    unsigned int Count  : 12;
#else
    unsigned int Bitmap;
    unsigned int Count;
#endif

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    BitmapHolder() { }

    BitmapHolder(unsigned int bitmap, unsigned int count) : 
            Bitmap(bitmap), Count(count) { }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool operator ==(const BitmapHolder& other) {
        return* ((unsigned int*)this) == *((unsigned int*)&other);
    }

    bool operator !=(const BitmapHolder& other) {
        return !this->operator ==(other);
    }
};

const BitmapHolder BitmapHolder::None = BitmapHolder(-1, 0);


// Creates a 32-bit mask that stores on each 2 bits the mapping 
// between a location and the corresponding subgroup.
// Replaces the expensive division that would have been necessary on each allocation.
struct SubgroupMapping {
    unsigned int Mask;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    SubgroupMapping() {}

    SubgroupMapping(unsigned int totalLoc, unsigned int locPerSubgroup) {
        Mask = 0;

        for(unsigned int i = 0; i < totalLoc; i++) {
            Mask |= (i / locPerSubgroup) << (i * 2);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int GetSubgroup(unsigned int index) {
        return (Mask >> (index * 2)) & 0x03;
    }
};


class LargeGroup : public LargeTraits::NodeType {
    static const unsigned int HEADER_SIZE = Constants::LARGE_GROUP_HEADER_SIZE;

public:
    // The first cache line contains only the data 
    // inherited from 'SmallTraits::NodeType'.
    char Padding1[Constants::CACHE_LINE_SIZE - sizeof(LargeTraits::NodeType)];
    // ------------------------------------ END OF CACHE LINE 1 ------------------------* 

    void* ParentBin;           // The owner of the group.
    void* ParentBlock;         // The block to which the group belongs.
    void* NextPublic;          // The next group that has public locations. 
    unsigned int ThreadId;     // The ID of the thread who owns this group.
    unsigned int Locations;    // The maximum number of locations that can be allocated.
    unsigned int LocationSize; // The size of a location in this group.
    unsigned int PrivateFree;
    unsigned int PrivateBitmap;
    SubgroupMapping Subgroups;

    // Padding to cache line.
    char Padding2[Constants::CACHE_LINE_SIZE - (3 *  sizeof(void*)) - 
                  (5 * sizeof(unsigned int)) - sizeof(SubgroupMapping)];
    // ------------------------------------ END OF CACHE LINE 2 ------------------------* 

    BitmapHolder PublicBitmap;

private:
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* LocationToAddress(unsigned int location) {
        return (void*)((uintptr_t)this + ((Subgroups.GetSubgroup(location) + 1) * HEADER_SIZE) + 
                       (LocationSize*  location));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int AddressToLocation(void* address) {
        return (unsigned int)(((uintptr_t)address - (uintptr_t)this) / LocationSize);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void Reset() {
        // Reset 4 bytes at a time.
        UnrolledSet<int, 0, 0, 
                    HEADER_SIZE / sizeof(int)>::Execute((int*)this);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void MergeBitmaps() {
        // Use atomic instructions to get the correct PublicStart 
        // and set it to Constants::LIST_END.
        BitmapHolder currentBitmap;
        BitmapHolder test = PublicBitmap;

        do	{
            currentBitmap = test;
            unsigned int temp = 
                    Atomic::CompareExchange((unsigned int*)&PublicBitmap, 
                                            *((unsigned int*)&BitmapHolder::None),
                                            *((unsigned int*)&currentBitmap));
            test = *reinterpret_cast<BitmapHolder*>(&temp);
        } while (test != currentBitmap);

        // 'location' now contains the correct public list start.
        // Add the number of elements in the public list to the private counter.
        PrivateBitmap |= currentBitmap.Bitmap;
        PrivateFree += currentBitmap.Count;
    }

public:
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Initializes a group that has all it's locations free.
    void InitializeUnused(unsigned int locationSize, unsigned int locations, 
                          unsigned int threadId) {
        ThreadId = threadId;
        LocationSize = locationSize;
        Locations = locations;
        PrivateFree = locations;
        PrivateBitmap = -1;

        Subgroups = SubgroupMapping(Locations, Locations / 4);

        // Each subgroup must be marked as being one.
        for(unsigned int i = 0; i < 4; i++) {
            auto subgroup = reinterpret_cast<LargeTraits::NodeType*>((uintptr_t)this + 
                                            (i * Constants::SMALL_GROUP_SIZE));
            LargeTraits::PolicyType::SetType(subgroup);
            LargeTraits::PolicyType::SetSubgroup(subgroup, i);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Initializes a group that has some of it's locations used.
    void InitializeUsed(unsigned int threadId) {
        ThreadId = threadId;

        // Make the public bitmap list private.
        if(PrivateFree != Locations) {
            MergeBitmaps();
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsEmptyEnough() {
        return PrivateFree > 0;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool CanBeStolen() {
        return PrivateFree >= (Locations / 4); // 25%
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool ShouldReturn() {
        // The group can return to the global pool if it
        // has more than 75% free locations.
        return (PrivateFree >= ((Locations*  3) / 4) && 
               (PublicBitmap == BitmapHolder::None));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsFull() {
        return (PrivateFree == Locations) && 
               (PublicBitmap == BitmapHolder::None);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool MayBeFull(unsigned int publicLocations) {
        return (PrivateFree + publicLocations == Locations);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool HasPublic() {
        return (PublicBitmap != BitmapHolder::None);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* GetPrivateLocation() {
        if(PrivateFree == 0) {
            return nullptr;
        }

        unsigned int location = Bitmap::SearchForward(PrivateBitmap);
        Bitmap::ResetBit(PrivateBitmap, location);
        PrivateFree--;
        return LocationToAddress(location);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* GetPublicLocation() {
        if(PublicBitmap == BitmapHolder::None) {
            return nullptr;
        }

        MergeBitmaps();
        return GetPrivateLocation();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* GetLocation() {
        // Try to allocate from the private locations first.
        void* address = GetPrivateLocation();
        
        if(address != nullptr) {
            return address;
        }
        
        return GetPublicLocation();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void ReturnPrivateLocation(void* address) {
        unsigned int location = AddressToLocation(address);
        Bitmap::SetBit(PrivateBitmap, location);
        PrivateFree++;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int ReturnPublicLocation(void* address) {
        unsigned int location = AddressToLocation(address);
        BitmapHolder currentBitmap;
        BitmapHolder replacement;
        BitmapHolder test = PublicBitmap;

        do {
            currentBitmap = test;
            replacement.Count = currentBitmap.Count + 1; 
            replacement.Bitmap = currentBitmap.Bitmap | (1 << location);

            unsigned int temp = 
                    Atomic::CompareExchange((unsigned int*)&PublicBitmap, 
                                            *((unsigned int*)&replacement),
                                            *((unsigned int*)&currentBitmap));
            test = *reinterpret_cast<BitmapHolder*>(&temp);
        } while (test != currentBitmap);

        return replacement.Count;
    }

    void PrivatizeLocations() {
        MergeBitmaps();
    }
};

} // namespace Base
#endif
