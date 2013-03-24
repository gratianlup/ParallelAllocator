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
// Implements a wrapper around the OS memory allocation system.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_MEMORY_HPP
#define PC_BASE_ALLOCATOR_MEMORY_HPP

#include "Statistics.hpp"
#include "ThreadUtils.hpp"

#ifdef PLATFORM_WINDOWS
    #include <Windows.h>
    #include <intrin.h>
#else
    static_assert(false, "Not yet implemented.");
#endif

namespace Base {

class Memory {
#if defined(PLATFORM_WINDOWS)
    // NUMA support.
    typedef LPVOID (WINAPI* VIRTUAL_ALLOC_EX_NUMA)(HANDLE, LPVOID, SIZE_T, 
                                                   DWORD, DWORD, DWORD);
    static const char* NAME_VIRTUAL_ALLOC_EX_NUMA;
    static VIRTUAL_ALLOC_EX_NUMA VirtualAllocExNumaFct;
#endif

public:
    // Allocates the specified amount of bytes from virtual memory.
    static void* Allocate(size_t size) {
        Statistics::BlockAllocated();

#if defined(PLATFORM_WINDOWS)
        return VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Allocates the specified amount of bytes from virtual memory.
    // Tries to allocate the memory from the specified NUMA node.
    static void* AllocateNuma(size_t size, unsigned int prefferedNode) {
        Statistics::BlockAllocated();

#if defined(PLATFORM_WINDOWS)
        if(VirtualAllocExNumaFct != nullptr) {
            // Under Vista+, allocate using the special NUMA method.
            return VirtualAllocExNuma(GetCurrentProcess(), nullptr, size,
                                      MEM_COMMIT, PAGE_READWRITE, prefferedNode);
        }
        else {
            return VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
        }
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Deallocates the data found at the given address.
    static void Deallocate(void* address) {
        Statistics::BlockDeallocated();

#if defined(PLATFORM_WINDOWS)
        VirtualFree(address, 0, MEM_RELEASE);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Deallocates the data found at the given address (NUMA version).
    static void DeallocateNuma(void* address, unsigned int prefferedNode) {
        Statistics::BlockDeallocated();
        
#if defined(PLATFORM_WINDOWS)
        VirtualFreeEx(GetCurrentProcess(), address, 0, MEM_RELEASE);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static unsigned int GetPageSize() {
#if defined(PLATFORM_WINDOWS)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static bool IsNumaSupported() {
#if defined(PLATFORM_WINDOWS)
        OSVERSIONINFOEX info;
        info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        GetVersionEx((LPOSVERSIONINFO)&info);

        if(info.dwMajorVersion >= 6) return true;     // Vista+
        if(info.dwMajorVersion == 5) {                // XP/2003
            if(info.dwMinorVersion == 1) {
                return (info.wServicePackMajor >= 2); // XP SP2
            }

            return (info.dwMinorVersion >= 2);        // XP64/2003;
        }

        return false;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static bool IsNumaAllocationSupported() {
#if defined(PLATFORM_WINDOWS)
        OSVERSIONINFOEX info;
        info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

        GetVersionEx((LPOSVERSIONINFO)&info);
        return info.dwMajorVersion >= 6; // Vista+
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void InitializeNumaAllocation() {
#if defined(PLATFORM_WINDOWS)
        if(IsNumaAllocationSupported()) {
            VirtualAllocExNumaFct = (VIRTUAL_ALLOC_EX_NUMA)
                                     GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
                                                    NAME_VIRTUAL_ALLOC_EX_NUMA);
        }
        else VirtualAllocExNumaFct = nullptr;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void FullBarrier() {
#if defined(PLATFORM_WINDOWS)
    #if defined(PLATFORM_64)
        __faststorefence();
    #else
        MemoryBarrier();
    #endif
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    static T ReadValue(volatile T* address) {
        T value = *address;
        FullBarrier();
        return value;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    static void WriteValue(volatile T* address, const T& value) {
        FullBarrier();
        *address = value;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void Prefetch(void* address) {
#if defined(PLATFORM_64)
    #if defined(PLATFORM_WINDOWS)
        _mm_prefetch((char*)address, _MM_HINT_NTA);
    #else
        static_assert(false, "Not yet implemented.");
    #endif
#endif
    }
};

#if defined(PLATFORM_WINDOWS)
    const char* Memory::NAME_VIRTUAL_ALLOC_EX_NUMA = "VirtualAllocExNuma";
    Memory::VIRTUAL_ALLOC_EX_NUMA Memory::VirtualAllocExNumaFct = 0;
#endif

} // namespace Base
#endif
