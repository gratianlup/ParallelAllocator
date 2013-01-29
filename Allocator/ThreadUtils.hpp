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
// Implements various thread-related methods.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_THREAD_UTILS_HPP
#define PC_BASE_ALLOCATOR_THREAD_UTILS_HPP

#ifdef PLATFORM_WINDOWS
    #include <Windows.h>
    #include <intrin.h>
#else
    static_assert(false, "Not yet implemented.");
#endif

namespace Base {

class ThreadUtils {
#if defined(PLATFORM_WINDOWS)
    typedef BOOL (WINAPI* GET_NUMA__HIGHEST_NODE_NUMBER)(PULONG);
    typedef BOOL (WINAPI* GET_NUMA_NODE_PROCESSOR_MASK)(UCHAR, PULONGLONG);
    static const char* NAME_GET_NUMA__HIGHEST_NODE_NUMBER;
    static const char* NAME_GET_NUMA_NODE_PROCESSOR_MASK;

    static GET_NUMA__HIGHEST_NODE_NUMBER GetNumaHighestNodeNumberFct;
    static GET_NUMA_NODE_PROCESSOR_MASK GetNumaNodeProcessorMaskFct;
#endif

public:
    static void InitializeNuma() {
#if defined(PLATFORM_WINDOWS)
        GetNumaHighestNodeNumberFct = (GET_NUMA__HIGHEST_NODE_NUMBER)
                                       GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
                                       NAME_GET_NUMA__HIGHEST_NODE_NUMBER);

        GetNumaNodeProcessorMaskFct = (GET_NUMA_NODE_PROCESSOR_MASK)
                                       GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
                                       NAME_GET_NUMA_NODE_PROCESSOR_MASK);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static unsigned int GetCurrentThreadId()	{
#if defined(PLATFORM_WINDOWS)
        return (unsigned int)::GetCurrentThreadId();
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static unsigned int GetCpuNumber() {
#if defined(PLATFORM_WINDOWS)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwNumberOfProcessors;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    inline static unsigned int GetCurrentCPUNumber() {
        // Get the processor ID using APIC.
        // http://software.intel.com/en-us/articles/intel-64-architecture-processor-topology-enumeration/
#if defined(PLATFORM_WINDOWS)
        __asm {
            mov eax, 1
            cpuid
            shr ebx, 24
            mov eax, ebx
        }
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static unsigned int GetHighestNumaNode() {
#if defined(PLATFORM_WINDOWS)
        unsigned int number;
        GetNumaHighestNodeNumber((PULONG)&number);
        return number;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static unsigned __int64 GetNumaNodeCpus(unsigned int node) {
#if defined(PLATFORM_WINDOWS)
        unsigned __int64 mask;
        if(GetNumaNodeProcessorMask((UCHAR)node, (PULONGLONG)&mask) == FALSE) {
            return 0;
        }

        return mask;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static unsigned int AllocateTLSIndex() {
#if defined(PLATFORM_WINDOWS)
        return TlsAlloc();
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Methods for Thread Local Storage (TLS)
    static void* GetTLSValue(unsigned int index) {
#if defined(PLATFORM_WINDOWS)
        return TlsGetValue(index);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void SetTLSValue(unsigned int index, void* data) {
#if defined(PLATFORM_WINDOWS)
        TlsSetValue(index, data);
#else
        static_assert(false, "Not yet implemented.");
#endif	
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void FreeTLSIndex(unsigned int index) {
#if defined(PLATFORM_WINDOWS)
        TlsFree(index);
#else
        static_assert(false, "Not yet implemented.");
#endif	
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void SwitchToThread() {
#if defined(PLATFORM_WINDOWS)
        ::SwitchToThread();
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void Wait() {
#if defined(PLATFORM_32)
        for(unsigned int i = 0; i < 64; i++) {
    #if defined(PLATFORM_WINDOWS)
            // Use the intrinsic because on 64-bit Visual C++
            // doesn't allow inline assembly.
            __nop();
    #else
            static_assert(false, "Not yet implemented.");
    #endif
#else
    #if defined(PLATFORM_WINDOWS)
        _mm_pause();
        _mm_pause();
    #else
        static_assert(false, "Not yet implemented.");
    #endif
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void SpinWait(unsigned int waitCount) {
        for(unsigned int i = 0; i < waitCount; i++) {
            ThreadUtils::Wait();
        }
        
        if(waitCount >= 1024) {
            // Give threads with a lower priority a chance to run.
            ThreadUtils::SwitchToThread();
        }
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Returns the system time in seconds.
    static unsigned int GetSystemTime() {
#if defined(PLATFORM_WINDOWS)
        unsigned int time = GetTickCount();
        time &= 0xFFFFFC00;
        time >>= 10;
        return time;
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Creates a thread that calls the specified function.
    static void* CreateThread(void* startAddress, void* param, 
                              size_t stackSize = 4 * 1024) {
#if defined(PLATFORM_WINDOWS)
        DWORD threadId;
        return ::CreateThread(nullptr, stackSize, (LPTHREAD_START_ROUTINE)startAddress, 
                              param, STACK_SIZE_PARAM_IS_A_RESERVATION, &threadId);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    inline static bool SetThreadLowPriority(void* threadHandle) {
#if defined(PLATFORM_WINDOWS)
        return SetThreadPriority((HANDLE)threadHandle, THREAD_PRIORITY_BELOW_NORMAL);
#else
        static_assert(false, "Not yet implemented.");
#endif`
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    static void Sleep(unsigned int milliseconds) {
#if defined(PLATFORM_WINDOWS)
        ::Sleep(milliseconds);
#else
        static_assert(false, "Not yet implemented.");
#endif
    }

#if defined(PLATFORM_WINDOWS)
    // Used to set the name of the thread.
    #pragma pack(push, 8)
    typedef struct __THREADNAME_INFO {
        DWORD dwType;	  // Must be 0x1000.
        LPCSTR szName;	  // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags;	  // Reserved for future use, must be zero.
    } THREADNAME_INFO;
    #pragma pack(pop)

    // Sets the name of the thread. Works only in Visual C++.
    static void SetThreadName(unsigned int threadId, const char* name) {
        // Initialize the structure.
        THREADNAME_INFO info;
        info.szName = name;
        info.dwType = 0x1000;
        info.dwThreadID = threadId;
        info.dwFlags = 0;

        // Set the name of the thread. We raise a special exception
        // that is caught only by Visual C++.
        __try {
            RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), 
                           (ULONG_PTR*)&info );
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
#endif
};

#if defined(PLATFORM_WINDOWS)
const char* ThreadUtils::NAME_GET_NUMA__HIGHEST_NODE_NUMBER = "GetNumaHighestNodeNumber";
const char* ThreadUtils::NAME_GET_NUMA_NODE_PROCESSOR_MASK = "GetNumaNodeProcessorMask";
ThreadUtils::GET_NUMA__HIGHEST_NODE_NUMBER ThreadUtils::GetNumaHighestNodeNumberFct = nullptr;
ThreadUtils::GET_NUMA_NODE_PROCESSOR_MASK ThreadUtils::GetNumaNodeProcessorMaskFct = nullptr;
#endif

} // namespace Base
#endif
