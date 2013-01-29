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
// Implements a list used for huge memory locations.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_WAY_LISTS_HPP
#define PC_BASE_ALLOCATOR_WAY_LISTS_HPP

#include "FreeObjectList.hpp"

namespace Base {

// Definitions for pointers to methods that operate on lists of huge locations.
// Replaces the virtual mechanism that would be necessary to transparently access
// different types of lists (MultiWayList<n>, OneWayList).
typedef ListNode* (*AddObjectFunct)(void*, ListNode*, unsigned int);
typedef ListNode* (*RemoveFirstFunct)(void*, int);


// Base class for huge location lists. 
// Provides the pointers to the methods that operate on the list.
class HugeLocationList {
public:
	AddObjectFunct AddObject;
	RemoveFirstFunct RemoveFirst;
};


// A class that stores unused huge locations in a series of internal lists.
// The internal lists are accessed using a hash value (the thread Id in this case).
#pragma push()
#pragma pack(1)
template <unsigned int WayCount>
class MultiWayList : public HugeLocationList {
public:
	FreeObjectList<> ways_[WayCount];

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	MultiWayList() { }

	MultiWayList(unsigned int capacity, 
                 AddObjectFunct AddFunct, 
                 RemoveFirstFunct RemoveFunct) {
		AddObject = AddFunct; 
		RemoveFirst = RemoveFunct;
		unsigned int capacityPerWay = capacity / WayCount;

		for(unsigned int i = 0; i < WayCount; i++) {
			ways_[i] = FreeObjectList<>(capacityPerWay);
		}
	}
};


// A class that stores unused huge locations in a single internal list.
class OneWayList : public HugeLocationList {
public:
	FreeObjectList<> way_;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	OneWayList() { }

	OneWayList(unsigned int capacity, 
               AddObjectFunct AddFunct, 
               RemoveFirstFunct RemoveFunct) {
		AddObject = AddFunct; 
		RemoveFirst = RemoveFunct;
		way_ = FreeObjectList<>(capacity);
	}
};
#pragma pop()

} // namespace Base
#endif
