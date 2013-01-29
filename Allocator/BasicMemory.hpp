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
// Implements the basic memory allocator that works on all systems.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_BASIC_MEMORY_HPP
#define PC_BASE_ALLOCATOR_BASIC_MEMORY_HPP

#include "Memory.hpp"
#include "ThreadUtils.hpp"

namespace Base {

template <class SmallBAType, class LargeBAType>
class BasicMemory {
public:
	void* AllocateMemory(size_t size, unsigned int prefferedNode) {
		return Memory::Allocate(size);
	}

	void DeallocateMemory(void* address, unsigned int prefferedNode) {
		Memory::Deallocate(address);
	}

	unsigned int GetCurrentCpu() { 
		return ThreadUtils::GetCurrentCPUNumber(); 
	}

	unsigned int GetCpuNumber() {
		return ThreadUtils::GetCpuNumber(); 
	}

	template <class T>
	void* GetGroup(unsigned int currentCpu, unsigned int currentThreadId) { 
        return nullptr; 
    }

	template <class T>
	void ReturnGroup(void* group, unsigned int parentCpu) { }

	template <class T>
	void SetBlockAllocator(void* allocator, unsigned int cpuIndex) { }

	template <class T>
	void BlockAvailable(unsigned int cpuIndex) { }

	template <class T>
	void BlockUnavailable(unsigned int cpuIndex) { };

	unsigned int GetCpuNode(unsigned int cpuIndex) { 
        return 0; 
    }

	bool IsNuma() { 
        return false; 
    }

	unsigned int GetNodeNumber() { 
        return 0; 
    }

	void Initialize() { }
};

} // namespace Base
#endif
