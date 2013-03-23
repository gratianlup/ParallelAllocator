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
// Implements the group used for small locations.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_GROUP_HPP
#define PC_BASE_ALLOCATOR_GROUP_HPP

#include "AllocatorConstants.hpp"
#include "UnrolledLoops.hpp"
#include <stdlib.h>
#include "FreeObjectList.hpp"
#include "ThreadUtils.hpp"
#include "Atomic.hpp"
#include "BitSpinLock.hpp"
#include "ListHead.hpp"

namespace Base {

// If 'sorting' is enabled, the location pointers are 16-bit integers
// that represent the index of the location.
// Otherwise, the pointers are the actual memory address of the location.
#if defined(SORT)
    typedef short LocationPtr;
#else
    typedef void* LocationPtr;
#endif


// Contains information about a location that has been freed. 8 bytes in size.
struct LocationInfo {
    LocationPtr Location;
    void* Address;

    LocationInfo(LocationPtr location, void* address) :
            Location(location), Address(address) {}
};


// Describes a location that has been stolen.
#pragma pack(push)
#pragma pack(1)
struct StolenLocation {
    unsigned short Free;

    // The position of the active range is stored in the first 15 bits 
    // of the spinlock. The highest bit (bit 15) stores the lock state.
    BitSpinLock<unsigned short, 15> Position;
};


// Describes a range of stolen location that have the same size.
struct StolenRange {
    char Number;
    char Freed;

    // The alignment is stored in the upper 2 bits of 'Size' as a multiple of 4.
    unsigned short Size;

    unsigned int GetSize() { 
        return Size & 0x1FFF; 
    }

    void SetSize(unsigned int size) { 
        // No mask because it's set only once in the initialization.
        Size = size; 
    }

    bool IsLast() { 
        return (Size & 0x8000) != 0; 
    }

    void SetLast() { 
        Size |=  0x8000; 
    }

    void ResetLast() { 
        Size &= ~0x8000; 
    }
    
    bool IsEmpty() { 
        return Freed == Number; 
    }

    unsigned int GetAlignment() { 
        // The alignment is stored in the upper 2 bits of 'Size' as a multiple of 4.
        // to multiply by 4.
        return ((unsigned int)Size & 0x6000) >> 11; 
    }

    void SetAlignment(unsigned int alignment) {
        Size = (alignment << 11) | (Size & ~0x6000); 
    }
};


// Describes a location that has been freed.
struct FreedLocation {
    LocationPtr Next; // The next location in the list.

#if defined(SORT)
    char Bitmap[6]; // 48 bits.
#endif
};
#pragma pack(pop)


#pragma pack(push)
#pragma pack(1) // Make sure the compiler doesn't change the layout of the structures.
struct Group : public SmallTraits::NodeType {
private:
    // The header consists of three cache lines.
    // The first contains general information, like which are the owners of the group.
    // Data form the second line keeps track of the locations of the group, including
    // the ones that where freed by the owning thread.
    // The third line keeps track of locations freed by foreign threads, 
    // and if stealing is enabled, holds an array of locks to which ranges 
    // of stolen locations are mapped.
    static const unsigned int HEADER_SIZE = Constants::SMALL_GROUP_HEADER_SIZE;
    static const unsigned int INVALID_INDEX = -1;

#if defined(SORT)
    // This constants are needed only when sorting is enabled.
    static const int SET_SIZE          = 43;
    static const int LOCATIONS_PER_SET = 48;
    static const int BITMAP_START      = 2;  // After the 'Next' pointer field (2 bytes).
    static const int BITMAP_SIZE       = 6;  // 48 bits.
    static const int MERGE_THRESHOLD   = 16;
#endif

public:
    // The first cache line contains only the data inherited from 'SmallTraits::NodeType'.
    char Padding1[Constants::CACHE_LINE_SIZE - sizeof(SmallTraits::NodeType)];
    // ------------------------------------ END OF CACHE LINE 1 ------------------------* 

    void* ParentBin;   // The owner of the group.
    void* ParentBlock; // The block to which the group belongs.
    void* Stolen;      // The active location from which other bins steal smaller locations.
    unsigned int ThreadId;       // The ID of the thread who owns this group.
    unsigned int Locations;      // The maximum number of locations that can be allocated from this group.
    unsigned int LocationSize;   // The size of a location in this group.
    unsigned int SmallestStolen; // The number of the bin with the smallest location size that stole from this group.

    // Padding to cache line.
    char Padding2[Constants::CACHE_LINE_SIZE - 
                  (3 * sizeof(void*)) - (4 * sizeof(unsigned int))];
    // ------------------------------------ END OF CACHE LINE 2 ------------------------* 

    // The fields are split in two cache lines so that no cache coherency problems
    // occur when the group is accessed by two or more threads at the same time. 
    // The second cache line contains fields that can be modified only by the thread
    // that owns the group, but can be read by all threads. The third line contains
    // only fields that are modified by foreign threads. 
    LocationPtr CurrentLocation; // The last allocated location.
    LocationPtr PrivateStart;    // The index of the first location in the private free list.
    LocationPtr PrivateEnd;      // The index of the last location in the private free list.

#if defined(SORT)
    unsigned __int64 PrivateSetsBitmap; // Tracks which private location sets are used.
    unsigned short PrivateUsed;         // The number of used locations, without the public ones.
    char PrivateSets[SET_SIZE];         // The array used to keep track of freed locations by the owner thread.

    // Padding to cache line.
    char Padding2[Constants::CACHE_LINE_SIZE - 
                  (3 * sizeof(LocationPtr)) - (1 * sizeof(unsigned __int64)) -
                  (1 * sizeof(unsigned short)) - SET_SIZE];
#else
    LocationPtr  LastLocation;         // The address of the last possible location.
    unsigned int PrivateUsed;          // The number of used locations, without the public ones.

    // Padding to cache line.
    char Padding3[Constants::CACHE_LINE_SIZE -
                  (4 * sizeof(LocationPtr)) - (1 * sizeof(unsigned int))];
#endif
    // ------------------------------------ END OF CACHE LINE 3 ------------------------*

    // The list of public locations is modified through atomic operations.
    // (it's about 4 times faster than using a lock).
    ListHead<LocationPtr> PublicStart;
    void * NextPublic; // The next group that has public locations.

#if defined(STEAL) // Lock STILL NEEDED for stolen locations.
    int PublicLock; // A lock is needed for public stolen locations too.
#endif

private:
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Resets the header of the group (overwrites with 0).
    void Reset() {
#ifdef PLATFORM_32
        // Reset 4 bytes at a time under 32-bit.
        auto address = (unsigned int*)this;
        UnrolledSet<unsigned int, 0, 0, 
                    HEADER_SIZE / sizeof(unsigned int)>::Execute(address);
#else
        // Reset 8 bytes at a time under 64-bit.
        auto address = (unsigned __int64*)this;
        UnrolledSet<unsigned __int64, 0, 0, 
                    HEADER_SIZE / sizeof(unsigned __int64)>::Execute(address);
#endif
    }

    // Converts the given location to it's memory address.
    void* LocationToAddress(LocationPtr location) {
#if defined(SORT)
        return (void*)((char*)this + HEADER_SIZE + (location*  LocationSize));
#else
        return location;
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Converts the given memory address to the location
    // relative to this group.
    LocationPtr AddressToLocation(void* address)	{
#if defined(SORT)
        return (LocationPtr)(((uintptr_t)address - HEADER_SIZE - 
                              (uintptr_t)this) / LocationSize);
#else
        return address;
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Extracts the next location from the freed location 
    // found at the specified address.
    LocationPtr GetNextLocation(void* address) {
#if defined(SORT)
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);
        return freedLoc->Next;
#else
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);
        return freedLoc->Next;
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Sets the next location in the freed found at the specified address.
    void SetNextLocation(void* address, LocationPtr location) {
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);
        freedLoc->Next = location;
    }

#if defined(SORT)
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the set to which the given location belongs.
    int LocationSet(int location) {
        return location / LOCATIONS_PER_SET;
    }

    // Returns the position of the location in a set.
    int LocationInSet(int location) {
        return location % LOCATIONS_PER_SET;
    }
#endif

#if defined(SORT)
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Determines whether the private and public lists should be merged.
    bool ShouldMerge() {
        return PublicFree >= MERGE_THRESHOLD;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Adds the specified set to the bitmap.
    void AddSetToBitmap(unsigned __int64* bitmap, unsigned int set) {
        *bitmap |= 1ULL << set;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Removes the specified set to the bitmap.
    void RemoveSetFromBitmap(unsigned __int64* bitmap, unsigned int set) {
        *bitmap &= ~(1ULL << set);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the number of freed locations 
    // using the bitmap from the specified location.
    unsigned int FreedLocationNumber(FreedLocation* freedLoc) {
    #ifdef PLATFORM_32
        unsigned int val = *((unsigned int*)freedLoc->Bitmap);
        unsigned int ct = Bitmap::NumberOfSetBits(val);

        val = *((short unsigned int*)freedLoc->Bitmap + 2);
        ct += Bitmap::NumberOfSetBits(val);
        return ct;
    #else
        unsigned __int64 val = *((unsigned __int64*)freedLoc->Bitmap) & 0xFFFFFFFFFFFF0000;
        return Bitmap::NumberOfSetBits64(val);
    #endif
    }
#endif

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a location from the list of private locations.
    void* GetListLocation()	{
        void* address = LocationToAddress(PrivateStart);

#if defined(SORT)
        // The location needs to be removed from the bitmap.
        RemoveFromBitmap(PrivateSets, &PrivateSetsBitmap, PrivateStart);
#endif
        // Remove the location from the list of free ones.
        PrivateStart = GetNextLocation(address);
        PrivateUsed++;

        if(PrivateUsed == 0)	{
            // Set the last location in the list.
            // Used when merging with the public list.
            PrivateEnd = Constants::LIST_END;
        }

        return address;
    }
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Removes the specified location from the corresponding set array and bitmap.
#if defined(SORT)
    void RemoveFromBitmap(char* setArray, unsigned __int64* setBitmap, 
                          LocationPtr location) {
        LocationPtr bitmapHolder = GetBitmapHolder(setArray, location);
        void locationAddr = LocationToAddress(bitmapHolder);
        FreedLocation* freedLoc = static_cast<FreedLocation*>(locationAddr);

        if(bitmapHolder == INVALID_INDEX) {
          return; // It's the first time the location is allocated.
        }
        else if(bitmapHolder != location) {
            // Mark the location as reallocated.
            ResetBitmapLocationState(freedLoc, LocationInSet(location));
        }
        else {
            // This location is the one that holds the bitmap 
            // that describes the entire set.
            if(FreedLocationNumber(freedLoc) >= 2)	{
                // Mark the location as reallocated.
                ResetBitmapLocationState(freedLoc, LocationInSet(location));

                // We need to make sure the bitmap is not lost, so we copy it 
                // to the last freed location that is part of this set.
                void* locationAddr = LocationToAddress(bitmapHolder);
                unsigned int offset = LOCATIONS_PER_SET * LocationSet(bitmapHolder);
                unsigned int lastLocation = offset + BitmapLastFree(locationAddr, 
                                                                    LOCATIONS_PER_SET - 1);

                CopyLocationBitmap(LocationToAddress(lastLocation), freedLoc);
                SetBitmapHolder(setArray, lastLocation);
            }
            else {
                // This is the last location in the set.
                // We just need to mark that all locations are used.
                ResetBitmapHolder(setArray, bitmapHolder);
                RemoveSetFromBitmap(setBitmap, LocationSet(bitmapHolder));
            }
        }
    }
#endif

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Sets the data of the specified freed location to 0.
    void ResetLocation(void* address) {
        // It's enough to set only the first 8 bytes to 0, 
        // because more is never used.
#ifdef PLATFORM_32
        UnrolledSet<unsigned int, 0, 0, 
                    sizeof(FreedLocation) / sizeof(unsigned int)>::Execute(address);
#else
        UnrolledSet<__int64, 0, 0, 
                    sizeof(FreedLocation) / sizeof(__int64)>::Execute(address);
#endif
    }

#if defined(SORT)
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Copies the bitmap from the freed source location to the destination one.
    void CopyLocationBitmap(void* dest, void* source) {
        // Save the 'Next' pointer of the destination.
        unsigned short dstNext = ((FreedLocation*)dest)->Next;

    #ifdef PLATFORM_32
        UnrolledCopy<int, 0, sizeof(FreedLocation) / sizeof(int)>::Execute(dest, source);
    #else
        UnrolledCopy<__int64, 0, sizeof(FreedLocation) / sizeof(__int64)>::Execute(dest, source);
    #endif

        // Restore the 'Next' pointer.
        ((FreedLocation*)dest)->Next = dstNext;
    }

    // Merges the bitmap of the freed source location and  destination one, 
    // and stores it in destination.
    void MergeLocationBitmap(void* dest, void* source) {
        // Save the 'Next' pointer of the destination.
        unsigned short dstNext = ((FreedLocation*)dest)->Next;

    #ifdef PLATFORM_32
        UnrolledOr<int, 0, sizeof(FreedLocation) / sizeof(int)>::Execute(dest, source);
    #else
        UnrolledOr<__int64, 0, sizeof(FreedLocation) / sizeof(__int64)>::Execute(dest, source);
    #endif
        // Restore the 'Next' pointer.
        ((FreedLocation*)dest)->Next = dstNext;
    }

    // Finds the nearest location to the given one, that has been freed 
    // and is part of the given set, or of a set that comes before this one.
    unsigned int FindNearestFreedLocation(char* setArray, unsigned __int64* setBitmap, 
                                          unsigned int location, unsigned int locationSet,
                                          unsigned int& usedSet, unsigned int& bitmapHolder) {
        unsigned int startLocation = location;

        do {
            unsigned int result = setArray[locationSet];

            if(result != 0) {
                // We found a set that has freed locations.
                // Compute the index of the location that holds the bitmap.
                unsigned int offset = LOCATIONS_PER_SET*  locationSet;

                // We need to subtract 1 because the location is stored as:
                // Location + 1 (0 means the set has no freed locations).
                result = offset + (result - 1); 

                // Check if there is a freed location before this one in the set.
                unsigned int firstFreed = BitmapLastFree(LocationToAddress(result), 
                                                         LocationInSet(startLocation));
                if(firstFreed != INVALID_INDEX) {
                    // Return the found location.
                    bitmapHolder = result;
                    usedSet = locationSet;
                    return firstFreed + offset;
                }
            }

            // Go back to the first set with freed locations. If no one is found, 
            // FindLastUsedSet returns -1, // so it's guaranteed that we leave the loop.
            locationSet--;

            if(locationSet >= 0) {
                startLocation = LOCATIONS_PER_SET - 1;

                if(setArray[locationSet] == 0) {
                    locationSet = Bitmap::SearchReverse(*setBitmap, locationSet);
                }
            }
        }
        while(locationSet >= 0);
        
        // No freed location was found.
        usedSet = INVALID_INDEX;
        return INVALID_INDEX;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Sets the location that holds the bitmap used to keep track
    // of used location in its set.
    void SetBitmapHolder(char* setArray, unsigned int location) {
        // Get the position of this location in it's set.
        // We add 1 because the location is represented in the form:
        // Location + 1 (0 means not used).
        unsigned int locationSet = LocationSet(location);
        setArray[locationSet] = (location % LOCATIONS_PER_SET) + 1;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Sets the value of the set to 0 (meaning no freed location is in the set).
    void ResetBitmapHolder(char* setArray, unsigned int location) {
        unsigned int locationSet = LocationSet(location);
        setArray[locationSet] = 0;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets the location that holds the bitmap used to keep track
    // of used location in its set.
    LocationPtr GetBitmapHolder(char* setArray, LocationPtr location) {
        unsigned int locationSet = LocationSet(location);

        // We need to subtract 1 because the location is represented 
        // in the form: Location + 1 (0 means not used).
        unsigned int holder = setArray[locationSet];

        if(holder == 0) {
            return INVALID_INDEX; // No freed location in the set.
        }

        return holder - 1 + (locationSet*  LOCATIONS_PER_SET);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets the bitmap associated with the given location as a 64-bit number.
    // Use for debugging only.
    unsigned __int64 GetBitmap(void* address)	{
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);

        unsigned __int64 low = *((unsigned int*)freedLoc->Bitmap);
        unsigned __int64 high = freedLoc->Bitmap[4];
        return (high << 32) | low;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets the state (freed or used) of the given location from the associated bitmap.
    unsigned int GetBitmapLocationState(void* address, unsigned int location) {
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);
        unsigned __int64 bitmap = *((unsigned __int64*)freedLoc->Bitmap);
        return (bitmap &  (1ULL << location)) != 0;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Marks the location as freed in the bitmap associated 
    // with the location at the given address.
    void SetBitmapLocationState(void* address, unsigned int location) {
        // We need to make sure we don't write after the 6 bytes, 
        // because it could be the start of another location and this 
        // would overwrite it's data. We can't take the affected data 
        // and rewrite it along with the bitmap, because until we write it,
        // the data could have been changed by other threads!
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);
        unsigned __int64 bitmap = *((unsigned __int64*)freedLoc->Bitmap);
        bitmap |= 1ULL << location; // Set the bit.
        
        // Write the bitmap back to memory (low and high parts).
        *((unsigned int*)freedLoc->Bitmap) = (unsigned int)bitmap;
        *((unsigned short*)freedLoc->Bitmap + 2) = (unsigned short)(bitmap >> 32);
    }

    // Marks the location as used in the bitmap associated
    // with the location at the given address.
    void ResetBitmapLocationState(void* address, unsigned int location) {
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);
        unsigned __int64 bitmap = *((unsigned __int64*)freedLoc->Bitmap);
        bitmap &= ~(1ULL << location); // Reset the bit.

        // Write the bitmap back to memory (low and high parts).
        *((unsigned int*)freedLoc->Bitmap) = (unsigned int)bitmap;
        *((unsigned short*)freedLoc->Bitmap + 2) = (unsigned short)(bitmap >> 32);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the index of the first freed location that is before
    // the given 'startLocation'. If no such location could be found, 
    // INVALID_INDEX is returned.
    unsigned int BitmapLastFree(void* address, unsigned int startLocation) {
        FreedLocation* freedLoc = static_cast<FreedLocation*>(address);

        // Mask the first two bytes (not port of the bitmap).
        unsigned __int64 bitmap = *((unsigned __int64*)freedLoc->Bitmap) & ~0xFFFF;
        return Bitmap::SearchReverse(bitmap, startLocation);
    }
 
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Adds the freed location to the specified set array and bitmap.
    void AddFreedLocation(char* setArray, unsigned __int64* setBitmap, 
                          LocationInfo locInfo) {
        unsigned int locationSet = LocationSet(locInfo.Location);
        unsigned int bitmapHolder = GetBitmapHolder(setArray, locInfo.Location);

        if(bitmapHolder != INVALID_INDEX) {
            // We set the location the set points to 
            // and mark the location as used in the bitmap.
            SetBitmapLocationState(LocationToAddress(bitmapHolder), 
                                   LocationInSet(locInfo.Location));
        }
        else {
            // No location has been freed yet in this set.
            ResetLocation(locInfo.Address);
            SetBitmapHolder(setArray, locInfo.Location);
            SetBitmapLocationState(locInfo.Address, LocationInSet(locInfo.Location));

            AddSetToBitmap(setBitmap, locationSet);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to find the nearest location to which the one to be freed
    // should be linked in order to keep the list of freed locations 
    // "sorted" by the location address.
    void FreeLocationUsingBitmap(LocationInfo locInfo) {
        unsigned int locationSet = LocationSet(locInfo.Location);
        unsigned int bitmapHolder; 
        unsigned int previousSet;  

        unsigned int previous = FindNearestFreedLocation(PrivateSets, &PrivateSetsBitmap, 
                                                         locInfo.Location, locationSet, 
                                                         previousSet, bitmapHolder);

        // We found a freed location that is before this one!
        if(locationSet == previousSet) {
            // The previous location is in the same set as this one.
            SetBitmapLocationState(LocationToAddress(bitmapHolder), 
                                   LocationInSet(locInfo.Location));
        }
        else {
            AddFreedLocation(PrivateSets, &PrivateSetsBitmap, locInfo);
        }

        // Link this location to the one determined to be previous.
        void* parentAddress = LocationToAddress(previous);
        SetNextLocation(locInfo.Address, GetNextLocation(parentAddress));
        SetNextLocation(parentAddress, locInfo.Location);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Adds all the locations found in the specified 
    // list to the private list (sorted).
    void FreeLocationList(ListHead& location) {
        // All locations that were public need now to be inserted in the private list
        // (this preserves the property that the locations are always sorted).
        LocationPtr current = (LocationPtr)location.First;
        while(current != Constants::LIST_END) {
            // Obtain the next location first because it's going 
            // to be modified by 'FreeLocationUsingBitmap'.
            LocationPtr next = GetNextLocation(LocationToAddress(current)); 
            FreeLocationUsingBitmap(LocationInfo(current, LocationToAddress(current)));
            current = next;
        }
    }
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Copies the public list to the private one.
    void CopyFreeLists() {
        // Use atomic instructions to get the correct PublicStart 
        // and set it to Constants::LIST_END.
        ListHead<LocationPtr> location;
        ListHead<LocationPtr> test = PublicStart;

        do {
            location = test;
            auto listEnd = &ListHead<LocationPtr>::ListEnd;
            unsigned __int64 temp = 
                    Atomic::CompareExchange64((unsigned __int64*)&PublicStart, 
                                              *((unsigned __int64*)listEnd),
                                              *((unsigned __int64*)&location));
            test = *reinterpret_cast<ListHead<LocationPtr>*>(&temp);
        } while (test != location);

        // 'location' now contains the correct public list start.
        // Add the number of elements from the public list to the private counter.
        PrivateStart = (LocationPtr)location.GetFirst();
        PrivateUsed -= location.GetCount();

#if defined(SORT)
        FreeLocationList(location);
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Merges the public and private lists, keeping them sorted 
    // by the address of the locations.
    void MergeFreeLists() {
        // Link the public list to the end of the private list.
        // Use atomic instructions to get the correct PublicStart 
        // and set it to Constants::LIST_END.
        ListHead<LocationPtr> location;
        ListHead<LocationPtr> test = PublicStart;

        do {
            location = test;
            auto listEnd = &ListHead<LocationPtr>::ListEnd;
            unsigned __int64 temp = 
                    Atomic::CompareExchange64((unsigned __int64*)&PublicStart, 
                                              *((unsigned __int64*)listEnd),
                                              *((unsigned __int64*)&location));
            test = *reinterpret_cast<ListHead<LocationPtr>*>(&temp);
        } while (test != location);

        // Link the public list to the end of the private one.
        SetNextLocation(LocationToAddress(PrivateEnd), location.GetFirst());
        PrivateUsed -= location.GetCount();

#if defined(SORT)
        FreeLocationList(location);
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Computes the required alignment for the specified size.
    // 'size' multiple of 16 => 16 bytes alignment, else 8 byte alignment.
    unsigned int GetLocationAlignment(unsigned int size) {
        return 8 + (8 & (((size & 0xF) - 1) >> 31));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Computes the alignment of the specified range, in order to maintain 
    // the alignment required by the location. Adds between 0 and 12 bytes 
    // between the range header and the first location.
    unsigned int GetRangeAlignment(StolenRange* range, unsigned int size) {
        unsigned int offset;
        unsigned int alignment = GetLocationAlignment(size);
        unsigned int position = (size_t)range + sizeof(StolenRange);
        unsigned int alignedPostion = position;

        alignedPostion = (alignedPostion + (alignment - 1)) & ~(alignment - 1);
        offset = alignedPostion - position;
        return offset;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Creates and initializes a new range at the specified address.
    void CreateStolenRange(StolenRange* range, unsigned int size, 
                           unsigned int alignment) {
        range->Number = 0;
        range->Freed = 0;
        range->SetSize(size);
        range->SetAlignment(alignment);
        range->SetLast();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Computes the size of the specified range. 
    // Includes the header and alignment bytes.
    unsigned int GetRangeSize(StolenRange* range) {
        unsigned int alignment = range->GetAlignment();
        return (range->GetSize() * range->Number) + sizeof(StolenRange) + alignment;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets a location from the specified range. 
    // It assumes the obtained location remains within the stolen location.
    void* AllocateFromRange(StolenRange* range) {
        void* address = (void*)((char*)range + GetRangeSize(range));
        range->Number++;
        return address;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Creates and initializes a stolen location at the specified address.
    // It also allocates a location having the specified 'size'.
    void* InitializeStolen(void* location, unsigned int size) {
        // Create the first StolenRange structure.
        auto stolen = reinterpret_cast<StolenLocation*>(location);
        auto rangeAddr = (char*)stolen + sizeof(StolenLocation);
        auto range = reinterpret_cast<StolenRange*>(rangeAddr);

        CreateStolenRange(range, size, GetRangeAlignment(range, size));

        stolen->Position.SetLowPart(sizeof(StolenLocation));
        stolen->Free = LocationSize - size - 
                       sizeof(StolenLocation) - GetRangeSize(range);
        return AllocateFromRange(range);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the first range form the specified stolen locations. 
    // It assumes that this operation is valid!
    StolenRange* GetFirstRange(StolenLocation* stolen) {
        return reinterpret_cast<StolenRange*>((char*)stolen + sizeof(StolenLocation));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the next range, or nullptr if the specified range is the last one.
    StolenRange* GetNextRange(StolenRange* range) {
        if(range->IsLast()) {
            return nullptr;
        }

        return reinterpret_cast<StolenRange*>((char*)range + GetRangeSize(range));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a location that has been stoled to the source location.
    // If the source locations becomes empty, the method return 
    // it's address, else nullptr.
    void* ReturnStolen(void* address) {
        // 'LocationSize' = 12 is considered a special case.
        if(LocationSize != 12) {
            // Compute the starting address of the stolen location
            // from which this one was taken (it strips off the first bits 
            // of the address, corresponding to the offset in the location).
            auto startAddr = (uintptr_t)address - Constants::SMALL_GROUP_HEADER_SIZE - 
                             (uintptr_t)this;
            void* stolenAddress = (void*)((startAddr & ~((uintptr_t)LocationSize - 1)) + 
                                         (uintptr_t)this + Constants::SMALL_GROUP_HEADER_SIZE);

            // Synchronize access on this location.
            auto stolen = reinterpret_cast<StolenLocation*>(stolenAddress);
            BSLHolder<unsigned short, 15> lock(&stolen->Position);

            StolenRange* previous = nullptr;
            StolenRange* current = GetFirstRange(stolen);
            StolenRange* firstEmpty; // Keeps track of the last start of a series of empty ranges.
            unsigned int seriesSize; // Used to return the space from ranges that weren't 
                                     // last when they've been freed.

            // Walk from first to last range, until the one 
            // that holds the location is found.
            do {
                if((previous != nullptr) && previous->IsEmpty()) {
                    seriesSize += GetRangeSize(previous);

                    if(firstEmpty == nullptr) {
                        firstEmpty = previous; // The beginning of a new series was found.
                    }
                }
                else {
                    // An used location has been found, the series must be reset.
                    firstEmpty = nullptr;
                    seriesSize = 0;
                }

                char* rangeStart = (char*)current;
                char* rangeEnd = rangeStart + GetRangeSize(current);

                if(((char*)address < rangeEnd) && ((char*)address > rangeStart)){
                    // Found the required range!
                    current->Freed++;

                    if(current->IsEmpty() && current->IsLast()) {
                        if(firstEmpty == nullptr) {
                            // Make the previous range the active one.
                            stolen->Position.SetLowPart((char*)previous - (char*)stolen);
                            stolen->Free += GetRangeSize(current);
                        }
                        else {
                            // A series of ranges before this (last) one are free. 
                            // Move the position to the start of the series.
                            stolen->Position.SetLowPart((char*)firstEmpty - (char*)stolen);
                            stolen->Free += GetRangeSize(current) + seriesSize;
                        }
                    }

                    if(stolen->Position.GetLowPart() == sizeof(StolenLocation)) {
                        return current; // The location can be freed.
                    }
                    else return nullptr;
                }

                previous = current;
                current = GetNextRange(current);
            } while (current != nullptr);
        }
        else { // LocationSize == 12
            return address; // Nothing to do here, just return the received address.
        }
    }

public:
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Initializes a group that has all it's locations free.
    void InitializeUnused(unsigned int locationSize, unsigned int locations, 
                          unsigned int threadId) {
        void* tempBlock = ParentBlock;
        Reset();
        ParentBlock = tempBlock;

        ThreadId = threadId;
        LocationSize = locationSize;
        Locations = locations;
        PrivateStart = Constants::LIST_END;
        PublicStart = ListHead<LocationPtr>::ListEnd;
        SmallestStolen = Constants::NOT_STOLEN;

#if !defined(SORT)
        CurrentLocation = (char*)this + HEADER_SIZE;
        LastLocation    = (char*)this + HEADER_SIZE + (LocationSize*  Locations);
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Initializes a group that has some of it's locations used.
    void InitializeUsed(unsigned int threadId) {
        // Assign the new owner.
        ThreadId = threadId;
        SmallestStolen = Constants::NOT_STOLEN;

        // Make the public list private.
        if(PrivateStart == Constants::LIST_END) {
            CopyFreeLists();
        }
        else MergeFreeLists();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsEmptyEnough() {
#if defined(SORT)
        return (CurrentLocation < Locations) || 
               (PrivateUsed < Locations);
#else
        return PrivateUsed < Locations;
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool CanBeStolen() {
        return PrivateUsed <= ((Locations* 3) / 4); // 75%
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool ShouldReturn() {
        // The group can return to the global pool 
        // if it has more than 75% free locations.
        return (PrivateUsed <= (Locations / 4) && 
               (PublicStart == ListHead<LocationPtr>::ListEnd));
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsFull() {
        return (PrivateUsed == 0) && 
               (PublicStart == ListHead<LocationPtr>::ListEnd);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool MayBeFull(unsigned int publicLocations) {
        return (PrivateUsed - publicLocations == 0);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool HasPublic() {
        return (PublicStart != ListHead<LocationPtr>::ListEnd);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a location from the list of private ones.
    // If no location is available, nullptr is returned.
    void* GetPrivateLocation() {
#if defined(SORT)
        if(ShouldMerge())	{
            // If there are some freed location in the public list,
            // merge the two lists. Doing so we maximize the probability 
            // of having ordered locations.
            MergeFreeLists();
        }

        if(PrivateStart != Constants::LIST_END) {
            // Use the private list until it's empty.
            return GetListLocation();
        }
        else if(CurrentLocation < Locations) {
            // We still have free locations at the end of the group;
            RemoveFromBitmap(PrivateSets, &PrivateSetsBitmap, CurrentLocation);
            PrivateUsed++;
            return LocationToAddress(CurrentLocation++);
        }
#else	
        if(CurrentLocation < LastLocation) {
            // We still have free locations at the end of the group;
            void* address = CurrentLocation;
            CurrentLocation = (char*)CurrentLocation + LocationSize;
            PrivateUsed++;
            return address;
        }

        // No location at the end of the group is available, 
        // get from the list of freed locations.
        if(PrivateStart != Constants::LIST_END) {
            return GetListLocation();
        }
#endif
        return nullptr; // No free location could be found.
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns a location from the public list of freed locations.
    void* GetPublicLocation() {
        // If no private location is free, we merge the public 
        // and private free lists and try again to get a private 
        // location.  If the allocation fails the second time,
        // the group has no longer free locations.
        if(PublicStart == ListHead<LocationPtr>::ListEnd) {
            return nullptr;
        }

        // Just copy the public list into the private one 
        // (the private list is guaranteed to be empty).
        CopyFreeLists();
        return GetPrivateLocation();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* GetLocation() {
        // First try to allocate from the private locations.
        void* address = GetPrivateLocation();
        
        if(address != nullptr) {
            return address;
        }
        
        // Try to get a location from the public ones (needs synchronization).
        return GetPublicLocation();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void ReturnPrivateLocation(void* address) {
        assert(address != nullptr);
#if defined(STEAL)
        if(((size_t)address - Constants::SMALL_GROUP_HEADER_SIZE) % LocationSize != 0) {
            address = ReturnStolen(address);

            if(address == nullptr) {
                return; // The location is not completely free yet.
            }
        }
#endif
        LocationPtr location = AddressToLocation(address);

#if defined(SORT)
        // We don't need to call 'AddFreedLocation' here because
        // 'FreeLocationUsingBitmap' does it for us.
        FreeLocationUsingBitmap(LocationInfo(location, address));
#else
        SetNextLocation(address, PrivateStart);
        PrivateStart = location;

        if(PrivateEnd == Constants::LIST_END) {
            // This is the first location to be added in the list.
            PrivateEnd = location;
        }
#endif
        PrivateUsed--;
    }

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int ReturnPublicLocation(void* address) {
#if defined(STEAL)
        SpinLock publicLock(&PublicLock);

        // See if the location was stolen by another bin.
        if(((size_t)address - Constants::SMALL_GROUP_HEADER_SIZE) % LocationSize != 0) {
            publicLock.Lock();
            address = ReturnStolen(address);
            if(address == nullptr) return address; // The location is not empty yet.
            publicLock.Unlock(); // The lock is no longer needed.
        }
#endif
        // Use atomic instructions to insert the location into the public list.
        ListHead<LocationPtr> firstLocation;
        ListHead<LocationPtr> test = PublicStart;
        ListHead<LocationPtr> replacement(0, AddressToLocation(address));

        do	{
            firstLocation = test;

            // Increase the number of locations in the list.			
            replacement.SetCount(firstLocation.GetCount() + 1);
            SetNextLocation(address, firstLocation.GetFirst());

            unsigned __int64 temp = 
                    Atomic::CompareExchange64((unsigned __int64*)&PublicStart, 
                                              *((unsigned __int64*)&replacement),
                                              *((unsigned __int64*)&firstLocation));
            test = *reinterpret_cast<ListHead<LocationPtr>*>(&temp);
        } while (test != firstLocation);

        return replacement.GetCount();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Tries to steal a location having the specified 'size'. 
    // 'size' = 12 is considered a special case. If a location
    // couldn't be obtained from the active stolen one, the method 
    // is called recursively if it's still allowed to steal from the group.
    void* StealLocation(unsigned int size) {
        if(Stolen == nullptr) {
            // No stolen location is defined, try to get one.
            Stolen = GetLocation();

            if(Stolen == nullptr) {
                return nullptr;
            }

            if(LocationSize != 12) {
                // It is guaranteed that at least one time 
                // can be allocated from this stolen location.
                return InitializeStolen(Stolen, size);
            }
            else {
                // LocationSize = 12 is a special case.
                // We can fit only a 8 byte location, either at offset 0,
                // or at offset 4 in order to be properly aligned.
                void* stolenTmp = Stolen;
                Stolen = nullptr;

                if((unsigned int)stolenTmp % 8 == 0) {
                    return stolenTmp;
                }
                else return (void*)((char*)stolenTmp + sizeof(StolenLocation));
            }
        }

        // Synchronize access to this location.
        StolenLocation* stolen = reinterpret_cast<StolenLocation*>(Stolen);
        BSLHolder<unsigned short, 15> lock(&stolen->Position);

        if(stolen->Free >= size) {
            void* rangeAddress = (void*)((char*)stolen + stolen->Position.GetLowPart());
            StolenRange* range = reinterpret_cast<StolenRange*>(rangeAddress);

            if(range->GetSize() == size) {
                // The size of the range matches the requested size.
                // Take care to not assign more than 255 locations to the range.
                if(range->Number < 255) {
                    stolen->Free -= size;
                    return AllocateFromRange(range);
                }
                // => A new range will be created below.
            }

            // A new range needs to be created. Compute the position 
            // of the new range (after the current one).
            StolenRange* prevRange = range;
            unsigned int rangeOffset = GetRangeSize(range);
            range = reinterpret_cast<StolenRange*>((char*)range + rangeOffset);

            // Check if enough space is available.
            unsigned int alignment = GetRangeAlignment(range, size);

            if((stolen->Free - sizeof(StolenRange) - alignment) >= size) {
                // Initialize this range and allocate from it.
                prevRange->ResetLast();
                CreateStolenRange(range, size, alignment);

                // 'GetRangeSize' will return the size of an empty range.
                // Needs to be incremented atomically.
                stolen->Free -= size + GetRangeSize(range); 
                stolen->Position.AddLowPart(rangeOffset);
                return AllocateFromRange(range);
            }
        }

        // This location is full. Steal and allocate from another one 
        // (or return nullptr if none is found).
        Stolen = nullptr;

        if(CanBeStolen()) {
            return StealLocation(size);
        }
        else return nullptr;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void PrivatizeLocations() {
        if(PrivateStart != Constants::LIST_END) {
            MergeFreeLists();
        }
        else CopyFreeLists();
    }
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Debugging helpers
    void DumpStolen() {
        if(Stolen == nullptr) {
            return;
        }

        StolenLocation* stolen = (StolenLocation*)Stolen;
        StolenRange* range = (StolenRange*)((char*)stolen + 4);

        while(true)	{
            std::cout<<"Size: "<<range->GetSize()<<", Number: "
                     <<(int)range->Number<<", Freed: "
                     <<(int)range->Freed<<", Alignment: "
                     <<range->GetAlignment();
            std::cout<<"\n";

            if(range->IsLast()) {
                break;
            }
            
            range = (StolenRange*)((char*)range + GetRangeSize(range));
        }
    }

    LocationPtr Loc(void* a) { 
        return AddressToLocation(a); 
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void VerifyLocations() {
        LocationPtr loc = PrivateStart;
        
        while(loc != Constants::LIST_END) {
            LocationPtr next = GetNextLocation(LocationToAddress(loc));
            
            if(next != Constants::LIST_END && next < loc) {
                MessageBeep(-1);
            }
            
            loc = next;
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void DumpLocations() {
        LocationPtr loc = PrivateStart;
        
        while(loc != Constants::LIST_END) {
            std::cout<<loc<<" ";
            LocationPtr next = GetNextLocation(LocationToAddress(loc));
            assert(next == Constants::LIST_END || next > loc);
            loc = next;
        }

        std::cout<<"****** "<<ct<<"* *****";
        std::cout<<"\n\nPublic: ";
        loc = PublicStart;
        
        while(loc != Constants::LIST_END) {
            std::cout<<loc<<" ";
            loc = GetNextLocation(LocationToAddress(loc));
        }

        std::cout<<"\n--------------------------------------------------------\n";
    }
};
#pragma pack(pop)

} // namespace Base
#endif
