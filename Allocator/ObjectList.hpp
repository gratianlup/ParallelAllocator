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
// Implements a doubly-linked list of objects.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef PC_BASE_ALLOCATOR_OBJECT_LIST_HPP
#define PC_BASE_ALLOCATOR_OBJECT_LIST_HPP

#include "AllocatorConstants.hpp"

namespace Base {

// All objects that can be added to the list must derive from this class.
#pragma pack(push)
#pragma pack(1)
class ListNode {
public:
	ListNode* Next;
	ListNode* Previous;
};
#pragma pack(pop)


#pragma pack(push)
#pragma pack(1)
// Used by LargeGroup objects on 32 bit systems.
class ListNode32 {
public:
	ListNode32* Next;
	ListNode32* Previous;
	unsigned int Type;
	unsigned int Subgroup;
};
#pragma pack(pop)


// Policy used to handle operations on the Next and Previous pointers of a ListNode
// in the default case (used by small groups).
struct DefaultNodePolicy {
	static ListNode* GetNext(ListNode* node) {
		return node->Next;
	}

	static void SetNext(ListNode* node, ListNode* next) {
		node->Next = next;
	}

	static ListNode* GetPrevious(ListNode* node) {
		return node->Previous;
	}

	static void SetPrevious(ListNode* node, ListNode* previous) {
		node->Previous = previous;
	}
};


// Policy used with large groups. It packs two values (type and subgroup) into the
// most significant 3 bits of the Next pointer. 64 bit version.
struct LargeNodePolicy {
	static const unsigned int TypeIndex     = (sizeof(uintptr_t) * 8) - 1;
	static const uintptr_t TypeMask         = (intptr_t)1 << TypeIndex;
	static const unsigned int SubgroupIndex = (sizeof(uintptr_t) * 8) - 2;
	static const uintptr_t SubgroupMask     = ((uintptr_t)1 << SubgroupIndex) |
											  ((uintptr_t)1 << (SubgroupIndex - 1));
	static const uintptr_t DataMask         = TypeMask | SubgroupMask;
	static const uintptr_t PointerMask      = ~DataMask;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	static ListNode* GetNext(ListNode* node) {
		return reinterpret_cast<ListNode*>((uintptr_t)node->Next & PointerMask);
	}

	static void SetNext(ListNode* node, ListNode* next) {
		node->Next = reinterpret_cast<ListNode*>(((uintptr_t)node->Next&  DataMask) |
                                                  (uintptr_t)next);
	}

	static ListNode* GetPrevious(ListNode* node) {
		return node->Previous;     // Previous is not affected.
	}

	static void SetPrevious(ListNode* node, ListNode* previous) {
		node->Previous = previous; // Previous is not affected.
	}

	static unsigned int GetType(ListNode* node) {
		return ((uintptr_t)node->Next&  TypeMask) >> TypeIndex;
	}

	static void SetType(ListNode* node) {
		node->Next = reinterpret_cast<ListNode*>((uintptr_t)node->Next | TypeMask);
	}

	static void ResetType(ListNode* node) {
		node->Next = reinterpret_cast<ListNode*>((uintptr_t)node->Next&  ~TypeMask);
	}

	static unsigned int GetSubgroup(ListNode* node) {
		return ((uintptr_t)node->Next&  SubgroupMask) >> (SubgroupIndex - 1);
	}

	static void SetSubgroup(ListNode* node, unsigned int value) {
		node->Next = reinterpret_cast<ListNode*>(((uintptr_t)node->Next&  ~SubgroupMask) |
												   (uintptr_t)value << (SubgroupIndex - 1));
	}
};


// Policy used with large groups on 32 bit systems.
struct LargeNodePolicy32 {
	inline static ListNode32* GetNext(ListNode32* node) {
		return node->Next;
	}

	inline static void SetNext(ListNode32* node, ListNode32* next) {
		node->Next = next;
	}

	inline static ListNode32* GetPrevious(ListNode32* node) {
		return node->Previous;
	}

	inline static void SetPrevious(ListNode32* node, ListNode32* previous) {
		node->Previous = previous;
	}

	inline static unsigned int GetType(ListNode32* node) {
		return node->Type;
	}

	inline static void SetType(ListNode32* node) {
		node->Type = 1;
	}

	inline static void ResetType(ListNode32* node) {
		node->Type = 0;
	}

	inline static unsigned int GetSubgroup(ListNode32* node) {
		return node->Subgroup;
	}

	inline static void SetSubgroup(ListNode32* node, unsigned int value) {
		node->Subgroup = value;
	}
};


// By default it uses the 'DefaultNodePolicy' policy to handle the set/get operations
// on the Next and Previous pointers.
#pragma pack(push)
#pragma pack(1)
template <class NodeType = ListNode, class NodePolicy = DefaultNodePolicy>
class ObjectList {
protected:
	typedef typename NodePolicy Policy;
	typedef typename NodeType Node;

	NodeType* first_;
	NodeType* last_;
	unsigned int count_;

public:
	ObjectList() : first_(nullptr), last_(nullptr), count_(0) { }

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    NodeType* First() {
        return first_;
    }

    NodeType* Last() {
        return last_;
    }

    unsigned int Count() {
        return count_;
    }

    // Adds the specified node as the first in the list.
	void AddFirst(NodeType* node) {
		// Add to the front of the list.
		if(first_ == nullptr) {
			first_ = last_ = node;
			NodePolicy::SetNext(node, nullptr);
			NodePolicy::SetPrevious(node, nullptr);
		}
		else {
			NodePolicy::SetPrevious(node, nullptr);
			NodePolicy::SetNext(node, first_);
			NodePolicy::SetPrevious(first_, node);
			first_ = node;
		}

		count_++;
	}
	
    // Removes and returns the first node in the list,
    // or returns 'nullptr' if the list is empty.
	NodeType* RemoveFirst() {
		if(count_ > 0) {
			NodeType* node = first_;
			first_ = NodePolicy::GetNext(first_);

			if(first_ != nullptr)	{
				NodePolicy::SetPrevious(first_, nullptr);
			}
			else last_ = nullptr;

			count_--;
			return node;
		}

		return nullptr;
	}

    // Adds the specified node as the last in the list.
	void AddLast(NodeType* node) {
		NodePolicy::SetNext(node, nullptr);
		NodePolicy::SetPrevious(node, last_);

		if(first_ == nullptr) {
			first_ = node;
		}
		else NodePolicy::SetNext(last_, node);

		last_ = node;
		count_++;
	}

    // Removes and returns the last node in the list,
    // or returns 'nullptr' if the list is empty.
	NodeType* RemoveLast() {
		NodeType* node = nullptr;

		if(count_ > 0) {
			node = last_;

			if(first_ == last_) {
				first_ = last_ = nullptr;
			}
			else {
				NodeType* newLast = NodePolicy::GetPrevious(last_);
				NodePolicy::SetNext(newLast, nullptr);
				last_ = newLast;
			}

			count_--;
			return node;
		}

		return node;
	}

    // Adds the specified node after the first one.
	void AddAfter(NodeType* firstNode, NodeType* node) {
		NodeType* firstNext = NodePolicy::GetNext(firstNode);
		NodePolicy::SetPrevious(node, firstNode);
		NodePolicy::SetNext(node, firstNext);

		if(firstNext == nullptr) {
			last_ = node;
		}
		else NodePolicy::SetPrevious(firstNext, node);

		NodePolicy::SetNext(firstNode, node);
		count_++;
	}

    // Removes the specified node from the list.
	void Remove(NodeType* node) {
		NodeType* nodeNext = NodePolicy::GetNext(node);
		NodeType* nodePrevious = NodePolicy::GetPrevious(node);
		
		if(nodePrevious == nullptr) {
			first_ = nodeNext;
		}
		else NodePolicy::SetNext(nodePrevious, nodeNext);

		if(nodeNext == nullptr) {
			last_ = nodePrevious;
		}
		else NodePolicy::SetPrevious(nodeNext, nodePrevious);

		count_--;
	}

	int Count() {
		return count_;
	}
};
#pragma pack(pop)


// Traits used to select the policy and node type 
// based on the size of a pointer (32 or 64 bit).
template <unsigned int N = 0, bool Large = false> // Default case.
struct ListTraits {
	typedef ListNode          NodeType;
	typedef DefaultNodePolicy PolicyType;
};

template <>
struct ListTraits<4, false> {            // 32 bits, small groups.
	typedef ListNode32        NodeType;
	typedef LargeNodePolicy32 PolicyType;
};

template <>
struct ListTraits<4, true> {             // 32 bits, large groups.
	typedef ListNode32        NodeType;
	typedef LargeNodePolicy32 PolicyType;
};

template <>
struct ListTraits<8, true> {             // 64 bits, large groups.
	typedef ListNode        NodeType;
	typedef LargeNodePolicy PolicyType;
};

template <>
struct ListTraits<9, true> {             // Used for testing.
	typedef ListNode        NodeType;
	typedef LargeNodePolicy PolicyType;
};

typedef ListTraits<sizeof(void*), false> SmallTraits;
typedef ListTraits<sizeof(void*), true>  LargeTraits;

} // namespace Base
#endif
