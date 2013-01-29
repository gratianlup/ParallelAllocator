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
// Implements a pool of objects used by various modules.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_OBJECT_POOL_HPP
#define PC_BASE_ALLOCATOR_OBJECT_POOL_HPP

#include "Memory.hpp"
#include "SpinLock.hpp"
#include "Bitmap.hpp"
#include "AllocatorConstants.hpp"
#include "FreeObjectList.hpp"

namespace Base {

// Provides a pool of objects allocated directly from the OS.
// Objects are allocated in blocks with a maximum of 63 objects/block.
// Used to allocate block and thread descriptors (multiple descriptors
// will be allocated in the same page file => lesser chance of a page fault).
class ObjectPool : public ObjectList<> {
private:
    static const unsigned int BLOCK_HEADER_SIZE = Constants::CACHE_LINE_SIZE;

    // Nested types
    // Describes a block containing at most 63 objects.
    #pragma pack(1)
    struct BlockHeader : public ListNode {
        unsigned __int64 Bitmap; // Keeps track of the free objects.
        unsigned int FreeObjects;

        // Padding to cache line.
        char Padding[BLOCK_HEADER_SIZE - sizeof(ListNode) - 
                     sizeof(unsigned __int64) - sizeof(unsigned int)];
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int blockSize_; // Must be a number power of 2!
    unsigned int objectSize_;
    unsigned int cacheSize_;
    unsigned int lock_;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the maximum number of objects that can be stored in a block. 
    unsigned int MaxObjectNumber() {
         return (blockSize_ - BLOCK_HEADER_SIZE) / objectSize_;
    }

    // Initializes the specified block.
    void InitializeBlock(BlockHeader* block) {
        block->Bitmap = -1; // All divisions are available.
        block->FreeObjects = MaxObjectNumber();
    }

    // Allocates a new block and adds it to the list.
    void AllocateBlock() {
        // Allocate a new block of memory and add it to the list as the active one.
        auto block = reinterpret_cast<BlockHeader*>(Memory::Allocate(blockSize_));
        InitializeBlock(block);
        AddNewBlock(block);
    }

    void AddNewBlock(BlockHeader* block) {
        AddFirst(block);
    }

    // Tries to make the specified block the active one.
    // If the specified block has fever objects than the active one,
    // or the active one has more than 25% free objects, the block 
    // is put on the second position.
    void MakeBlockActive(BlockHeader* block) {
        if(First() == nullptr) {
            AddFirst(block); // The First() block in the list.
        }
        else {
            unsigned int firstFree = static_cast<BlockHeader*>(First())->FreeObjects;

            if((firstFree <= (MaxObjectNumber() / 4)) && 
               (block->FreeObjects > firstFree)) {
                // Few unused objects are in the active block, 
                // and this one has more unused objects,
                // so make it active.
                Remove(block);
                AddFirst(block);
            }
            else {
                // Add the block after the active one.
                Remove(block);
                AddAfter(First(), block);
            }
        }
    }

    // Removes the block from the list and deallocates it.
    void DeallocateBlock(BlockHeader* block) {
        Memory::Deallocate(block);
    }

    // Gets an unused object from the specified block.
    void* GetObjectFromBlock(BlockHeader* block)	{
        // Find the first available object.
        unsigned int objectIndex = Bitmap::SearchForward(block->Bitmap);
        
        // Mark the object as used and return it's address;
        block->Bitmap &= ~(1ULL << objectIndex);
        block->FreeObjects--;
        return (char*)block + BLOCK_HEADER_SIZE + (objectIndex*  objectSize_);
    }

    // Returns an unused object to the specified block.
    void ReturnObjectToBlock(BlockHeader* block, unsigned int objectOffset)	{
        // Mark the object as unused.
        unsigned int objectIndex = (objectOffset - BLOCK_HEADER_SIZE) / objectSize_;
        block->Bitmap |= 1ULL << objectIndex;
        block->FreeObjects++;
    }

public:
    ObjectPool() { }

    ObjectPool(unsigned int blockSize,unsigned  int divisionSize, 
               unsigned int cacheSize) : 
            ObjectList(), blockSize_(blockSize), 
            objectSize_(divisionSize), lock_(0), cacheSize_(cacheSize) { }

    ~ObjectPool() {
        // Acquire the lock. Will be automatically released by the destructor.
        SpinLock lock(&lock_);

        while(Count() > 0) {
            RemoveFirst();
            DeallocateBlock(static_cast<BlockHeader*>(First()));
        }
    }	

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Gets an object from the pool.
    void* GetObject()	{
        // Acquire the lock. Will be automatically released by the destructor.
        SpinLock lock(&lock_);
        
        if((Count() == 0) || (static_cast<BlockHeader*>(First())->FreeObjects == 0)) {
            // No free block is available, a new one needs to be allocated.
            AllocateBlock();
        }

        return GetObjectFromBlock(static_cast<BlockHeader*>(First()));
    }

    // Returns the specified object to the pool.
    void ReturnObject(void* address)	{
        // Acquire the lock. Will be automatically released by the destructor.
        SpinLock lock(&lock_);
        BlockHeader* block;
        unsigned int objectOffset;

        // It's not needed to search for the corresponding block, 
        // because it can be obtained from the address by mapping 
        // some of the first bits (depending on blockSize_).
#ifdef PLATFORM_32
        objectOffset = (unsigned int)address % blockSize_;
        block = reinterpret_cast<BlockHeader*>((unsigned int)address - objectOffset);
#else
        objectOffset = (unsigned int)((unsigned __int64)address % blockSize_);
        block = reinterpret_cast<BlockHeader*>((unsigned __int64)address - objectOffset);
#endif
        ReturnObjectToBlock(block, objectOffset);

        if(block != First()) {
            // Keep at least 'cacheSize_' blocks in the list.
            if((block->FreeObjects == MaxObjectNumber()) && 
               (Count() > cacheSize_) && 
               (static_cast<BlockHeader*>(First())->FreeObjects > 0)) {
                // This block can be returned to the OS.
                Remove(block);
                DeallocateBlock(block);
            }
            else {
                // Bring the block to the front of the list. This preserves 
                // the property that if the first block has no free objects, 
                // all the other ones don't have free objects either.
                MakeBlockActive(block);
            }
        }
    }
};

} // namespace Base
#endif
