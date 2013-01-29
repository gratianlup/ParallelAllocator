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
// Defines the structures used to manage huge locations.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_HUGE_LOCATION_HPP
#define PC_BASE_ALLOCATOR_HUGE_LOCATION_HPP

#include "Atomic.hpp"
#include "Stack.hpp"
#include <stdlib.h>
#include <math.h>

namespace Base {

// Huge locations that have been freed are cached (up to a limit
// specific to the location size), so that subsequent allocations 
// don't need to take the global lock of the OS-specific allocation routine.
// The size of the cache is inverse proportional with the size of the location 
// (smaller locations, that are used more often, have a bigger cache).
#pragma push()
#pragma pack(1)

// Huge locations have a size multiple of 4KB and are aligned
// on a 16 byte boundary. Locations between 12KB and 1MB are cached 
// (up to a limit) when they're freed by the client.
struct HugeBin; // Forward declaration.

struct HugeLocation : public ListNode {
    void* Address; // The address of the object (aligned to 16KB).
    HugeBin* Bin;  // The bin to which this location belongs.
    void* Parent;  // The location that is the parent of this one (if it has one).
    void* Block;   // The associated block header (if it has one).
    unsigned int Size; // The actual size of the object (requested by the client).
    volatile unsigned int References; // Keeps track of the groups obtained from unused memory.
    bool HasBlock;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void AddRef()  { 
        Atomic::Increment(&References); 
    }
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool Release() { 
        return Atomic::Decrement(&References) == 0; 
    }
};


// Contains the cached huge locations.
struct HugeBin {
    Stack<HugeLocation*> Cache;
    unsigned int CacheSize;
    unsigned int CacheTime;
    unsigned int MaxCacheSize;
    unsigned int ExtendedCacheSize;
    unsigned int CacheFullHits;
    
     // Align to cache line.
    char Padding[Constants::CACHE_LINE_SIZE - 
                 sizeof(Stack<HugeLocation*>) - (5 * sizeof(int))];
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void IncreaseCacheSize() {
        // Increase the size of the cache if the demand is very high.
        if(Atomic::Increment((unsigned int*)&CacheFullHits) % 4 == 0) {
            CacheSize = std::min(CacheSize + 1, ExtendedCacheSize);
            Cache.SetMaxObjects(CacheSize);
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void DecreaseCacheSize() {
        // NOT ATOMIC!!!
        if(CacheSize > MaxCacheSize) {
            CacheSize = std::max(MaxCacheSize, (CacheSize + MaxCacheSize) / 2);
            Cache.SetMaxObjects(CacheSize);
        }
    }   
};
#pragma pop()

} // namespace Base
#endif
