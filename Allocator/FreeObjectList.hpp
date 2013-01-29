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
// * The name "DocumentClustering" must not be used to endorse or promote
// products derived from this software without prior written permission.
//
// * Products derived from this software may not be called "DocumentClustering" nor
// may "DocumentClustering" appear in their names without prior written
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
// Implements a doubly-linked list that tracks free objects.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_FREE_OBJECT_LIST_HPP
#define PC_BASE_ALLOCATOR_FREE_OBJECT_LIST_HPP

#include "ObjectList.hpp"
#include "AllocatorConstants.hpp"
#include "SpinLock.hpp"

namespace Base {

#pragma pack(1)
template <class NodeType = ListTraits<>::NodeType, 
          class NodePolicy = ListTraits<>::PolicyType>
class FreeObjectList : public ObjectList<NodeType, NodePolicy> {
private:
	unsigned int lock_;
	unsigned int maxObjects_;

public:
	FreeObjectList() : 
            ObjectList(), lock_(0), maxObjects_(0x7FFFFFFF) { }

	FreeObjectList(unsigned int maxObjects) : 
            ObjectList(), lock_(0), maxObjects_(maxObjects) { }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Tries to add the specified node to the list. If the maximum number 
    // of objects is reached, the object is not added and it's address 
    // is returned, otherwise 'nullptr' is returned.
    // Note that this doesn't take the lock!
	NodeType* AddObjectUnlocked(NodeType* node)	{
		if(Count < maxObjects_) {
   			AddFirst(node);
			return nullptr;
		}
		
		return node;
	}

    // Tries to add the specified node to the list. If the maximum number 
    // of objects is reached, the object is not added and it's address 
    // is returned, otherwise 'nullptr' is returned.
	NodeType* AddObject(NodeType* node)	{
        // Acquire the lock. Will be automatically released by the destructor.
		SpinLock headerLock(&lock_);
		return AddObjectUnlocked(node);
	}

	// Removes the specified object from the list.
	void RemoveObject(NodeType* node) {
        // Acquire the lock. Will be automatically released by the destructor.
		SpinLock headerLock(&lock_);
		Remove(node);
	}

	// Removes and returns the first object in the list.
	// Returns 'nullptr' if no object could be found.
	NodeType* RemoveFirst() {
        // Acquire the lock. Will be automatically released by the destructor.
		SpinLock headerLock(&lock_);
		return ObjectList::RemoveFirst();
	}

    // Removes and returns the first object in the list.
	// Returns 'nullptr' if no object could be found.
    // Note that this doesn't take the lock!
	NodeType* RemoveFirstUnlocked() {
		return ObjectList::RemoveFirst();
	}

	// Should be used to provide synchronization for the unlocked
    // variants of the add/remove methods.
	unsigned int* GetLockValue() {
		return &lock_;
	}

	// Removes the specified object from the list.
    // Note that this doesn't take the lock!
	void RemoveObjectUnlocked(NodeType* node) {
		Remove(node);
	}

	unsigned int GetMaxObjects() {
		return maxObjects_;
	}

	void SetMaxObjects(unsigned int value) {
		maxObjects_ = value;
	}
};

} // namespace Base
#endif
