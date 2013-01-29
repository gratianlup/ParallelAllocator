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
// Implements a module that collects various statistics.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_STATISTICS_HPP
#define PC_BASE_ALLOCATOR_STATISTICS_HPP

#include "Atomic.hpp"
#include <stdio.h>

namespace Base {

class Statistics {
public:
	static volatile unsigned int groupsObtained;
	static volatile unsigned int usedGroupsReturned;
	static volatile unsigned int emptyGroupsReturned;
	static volatile unsigned int invalidPublicGroups;
	static volatile unsigned int publicLocationFreed;
	static volatile unsigned int activeGroupChanged;
	static volatile unsigned int blocksAllocated;
	static volatile unsigned int blocksDeallocated;
	static volatile unsigned int broughtToFront;
	static volatile unsigned int threadsCreated;
	static volatile unsigned int threadsDestroyed;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#if defined(STATISTICS)
	static void GroupObtained(void* group) {
		Atomic::Increment(&groupsObtained);
	}

	static void UsedGroupReturned(void* group) {
		Atomic::Increment(&usedGroupsReturned);
	}

	static void EmptyGroupReturned(void* group)	{
		Atomic::Increment(&emptyGroupsReturned);
	}

	static void InvalidPublicGroup(void* group)	{
		Atomic::Increment(&invalidPublicGroups);
	}

	static void PublicLocationFreed(void* group) {
		Atomic::Increment(&publicLocationFreed);
	}

	static void ActiveGroupChanged(void* group) {
		Atomic::Increment(&activeGroupChanged);
	}

	static void BlockAllocated() {
		Atomic::Increment(&blocksAllocated);
	}

	static void BlockDeallocated() {
		Atomic::Increment(&blocksDeallocated);
	}

	static void BroughtToFront() {
		Atomic::Increment(&broughtToFront);
	}

	static void ThreadCreated() {
		Atomic::Increment(&threadsCreated);
	}

	static void ThreadDestroyed() {
		Atomic::Increment(&threadsDestroyed);
	}
#else
    // No statistics collected.
	static void GroupObtained(void* group) {}
	static void UsedGroupReturned(void* group) {}
	static void EmptyGroupReturned(void* group) {}
	static void InvalidPublicGroup(void* group) {}
	static void PublicLocationFreed(void* group) {}
	static void ActiveGroupChanged(void* group) {}
	static void BlockAllocated() {}
	static void BlockDeallocated() {}
	static void BroughtToFront() {}
	static void ThreadCreated() {}
	static void ThreadDestroyed() {}
#endif

	static void DisplayInt(unsigned int value, char* message) {
		printf("%25s: %d\n", message, value);
	}

	static void Display() {
		DisplayInt(blocksAllocated*  Constants::BLOCK_SIZE, "Memory allocated");
		DisplayInt(blocksAllocated,     "Blocks allocated");
		DisplayInt(blocksDeallocated,   "Blocks deallocated");
		DisplayInt(groupsObtained,      "Groups obtained");
		DisplayInt(usedGroupsReturned,  "Groups returned (used)");
		DisplayInt(emptyGroupsReturned, "Groups returned (empty)");
		DisplayInt(invalidPublicGroups, "Invalid public groups");
		DisplayInt(publicLocationFreed, "Public locations");
		DisplayInt(activeGroupChanged,  "Active group changed");
		DisplayInt(broughtToFront,      "Brought to front");
		DisplayInt(threadsCreated,      "Threads created");
		DisplayInt(threadsDestroyed,    "Threads destroyed");
	}
};


// Default values.
volatile unsigned int Statistics::groupsObtained = 0;
volatile unsigned int Statistics::usedGroupsReturned = 0;
volatile unsigned int Statistics::emptyGroupsReturned = 0;
volatile unsigned int Statistics::invalidPublicGroups = 0;
volatile unsigned int Statistics::publicLocationFreed = 0;
volatile unsigned int Statistics::activeGroupChanged = 0;
volatile unsigned int Statistics::blocksAllocated = 0;
volatile unsigned int Statistics::blocksDeallocated = 0;
volatile unsigned int Statistics::broughtToFront = 0;
volatile unsigned int Statistics::threadsCreated= 0;
volatile unsigned int Statistics::threadsDestroyed= 0;

} // namespace Base
#endif
