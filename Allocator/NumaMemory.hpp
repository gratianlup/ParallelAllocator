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
// Defines a helper for manipulating bit maps.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_NUMA_MEMORY_HPP
#define PC_BASE_ALLOCATOR_NUMA_MEMORY_HPP

#include "Memory.hpp"
#include "ThreadUtils.hpp"
#include "AllocatorConstants.hpp"
#include "Bitmap.hpp"

namespace Base {

template <class SmallBAType, class LargeBAType>
class NumaMemory {
private:
    typedef typename NumaMemory<SmallBAType, LargeBAType> PolicyType;
    static const unsigned int MAX_CPU = 64;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // Nested types
    #pragma pack(push)
    #pragma pack(1)
    struct NodeList {
        char Nodes[MAX_CPU];

        NodeList() {}

        NodeList(char* array, unsigned int n) {
            for(int unsigned i = 0; i < n; i++) {
                Nodes[i] = array[i];
            }
        }

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        void Sort(int n, unsigned int* hopArray) {
            // Speed is not important here, because this is done only once.
            // A basic selection sort is fast enough.
            for(unsigned int i = 0; i < n; i++) {
                for(unsigned int j = i + 1; j < n; j++) {
                    if(hopArray[i] < hopArray[j]) {
                        char t = hopArray[i];
                        hopArray[i] = hopArray[j];
                        hopArray[j] = t;
                    }
                }
            }
        }

        char operator[] (unsigned int index) {
            return Nodes[index];
        }
    };

    // Describes a NUMA node (allocators and nearest nodes).
    struct NumaNode {
        SmallBAType* SmallAllocator;
        LargeBAType* LargeAllocator;
        bool HasFreeSmallBlock;
        bool HasFreeLargeBlock;

        // Padding to cache line.
        char Padding[Constants::CACHE_LINE_SIZE - (2 * sizeof(void*)) - (2 * sizeof(bool))];
        NodeList NearestNodes;
    };
    #pragma pack(pop)

    // Selects the appropriate block allocator (small or large).
    template <class BAType>
    struct BASelector {
        typedef typename SmallBAType AllocType;
        typedef typename SmallBAType::GroupT GroupType;

        static AllocType* GetAllocator(NumaNode* node) {
            return node->SmallAllocator;
        }

        static void SetAllocator(NumaNode* node, AllocType* allocator) {
            node->SmallAllocator = allocator;
        }

        static bool GetFreeBlock(NumaNode* node) {
            return node->HasFreeSmallBlock;
        }

        static void SetFreeBlock(NumaNode* node, bool available) {
            node->HasFreeSmallBlock = available;
        }
    };

    template <>
    struct BASelector<LargeBAType> {
        typedef typename LargeBAType AllocType;
        typedef typename LargeBAType::GroupT GroupType;

        static AllocType* GetAllocator(NumaNode* node) {
            return node->LargeAllocator;
        }

        static void SetAllocator(NumaNode* node, AllocType* allocator) {
            node->LargeAllocator = allocator;
        }

        static bool GetFreeBlock(NumaNode* node) {
            return node->HasFreeLargeBlock;
        }

        static void SetFreeBlock(NumaNode* node, bool available) {
            node->HasFreeLargeBlock = available;
        }
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    unsigned int cpuNumber_;
    unsigned int nodeNumber_;
    unsigned int pageSize_;
    NumaNode nodes_[MAX_CPU];
    unsigned int cpuToNuma_[MAX_CPU];
    bool isNuma_;

public:
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void* AllocateMemory(size_t size, unsigned int prefferedNode) {
        void* address = Memory::AllocateNuma(size, prefferedNode);

        if((address != nullptr) && (nodeNumber_ > 0)) {
            // Touch the pages so that they're committed 
            // to the requested node (if it's possible).
            char* position = (char*)address;
            char* end = (char*)(((uintptr_t)address + size + pageSize_) & 
                               ~((uintptr_t)pageSize_ - 1));

            while(position < end) {
                // Force read using 'volatile'.
                *((volatile char*)position) = 0;
                position += pageSize_;
            }
        }

        return address;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void DeallocateMemory(void* address, unsigned int prefferedNode) {
        Memory::DeallocateNuma(address, prefferedNode);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int GetCurrentCpu() {
        return ThreadUtils::GetCurrentCPUNumber();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int GetCpuNumber() {
        return ThreadUtils::GetCpuNumber();
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    void* GetGroup(unsigned int currentNode, unsigned int currentThreadId) {
        // Find the nearest NUMA node with free blocks
        // and try to allocate from there.
        if(!isNuma_) {
            return nullptr;
        }

        NumaNode* info = &nodes_[currentNode];

        for(unsigned int i = 0; i < (nodeNumber_ - 1); i++) {
            NumaNode* victim =& nodes_[info->NearestNodes[i]];

            if(BASelector<T>::GetFreeBlock(victim)) {
                // Found a node with at least one free block.
                auto group = BASelector<T>::GetAllocator(victim)
                                ->TryGetGroup<PolicyType>(currentThreadId);
                if(group != nullptr) {
                    return group;
                }
            }
        }

        return nullptr; // Could not allocate.
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    void ReturnGroup(void* group, unsigned int parentNode) {
        NumaNode* info = &nodes_[parentNode];
        auto castedGroup = reinterpret_cast<BASelector<T>::GroupType*>(group);
        BASelector<T>::GetAllocator(info)
            ->ReturnFullGroup<PolicyType>(castedGroup, true);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    void SetBlockAllocator(typename BASelector<T>::AllocType* allocator, 
                           unsigned int nodeIndex) {
        BASelector<T>::SetAllocator(&nodes_[nodeIndex], allocator);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    void BlockAvailable(unsigned int nodeIndex) {
        BASelector<T>::SetFreeBlock(&nodes_[nodeIndex], true);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    template <class T>
    void BlockUnavailable(unsigned int nodeIndex) {
        BASelector<T>::SetFreeBlock(&nodes_[nodeIndex], false);
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int GetCpuNode(unsigned int cpuIndex) {
        return cpuToNuma_[cpuIndex];
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    bool IsNuma() {
        return isNuma_;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    unsigned int GetNodeNumber() {
        return nodeNumber_;
    }

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    void Initialize() {
        unsigned int maxNode = 0;
        nodeNumber_ = 0;
        cpuNumber_ = ThreadUtils::GetCpuNumber();
        isNuma_ = false;

        // First check if NUMA is available.
        if(Memory::IsNumaSupported()) {
            Memory::InitializeNumaAllocation();
            ThreadUtils::InitializeNuma();
            maxNode = ThreadUtils::GetHighestNumaNode();
        }

        if(maxNode == 0) {
            // This is not an NUMA system.
            for(unsigned int cpu = 0; cpu < cpuNumber_; cpu++) {
                cpuToNuma_[cpu] = 0;
            }

            return;
        }

        // This is a NUMA system, obtain information about each node.
        isNuma_ = true;

        for(unsigned int node = 1; node <= maxNode; node++) {
            unsigned __int64 nodeMask = ThreadUtils::GetNumaNodeCpus(node);

            if(nodeMask == 0) {
                continue; // The node is not valid.
            }

            do {
                unsigned int cpuIndex = Bitmap::SearchForward(nodeMask);
                cpuToNuma_[cpuIndex] = nodeNumber_;
                nodeMask = nodeMask & (nodeMask - 1); // Remove first set bit.
            } while(nodeMask != 0);

            nodeNumber_++;
        }
    }
};

} // namespace Base
#endif
