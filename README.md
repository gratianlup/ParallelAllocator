Parallel Allocator
===================

A memory allocator optimized for high concurrency and low memory consumption on modern multi-core and multi-processor systems.  
It includes special support for NUMA systems (stealing memory from another NUMA node, for example).

It is about 60% faster than the allocator included with Microsoft Visual C++ and uses less than half the memory for small objects  
(< 256 bytes), leading to better cache utilization; this also makes it ideal for applications that allocate a huge number of small objects.  
It is in many tests about 10% faster than the allocator included with Intel Thread Building Blocks.  

It is implemented in C++ with a small amount of x86 Assembly and was recently converted to C++11.  
Access to low-level synchronization and bit-manipulation instructions is done through compiler intrinsics.  

### Architecture overview:  

![SmartFlip screenshot](http://www.gratianlup.com/documents/allocator_summary.png)  

### Internal data structures details  

![SmartFlip screenshot](http://www.gratianlup.com/documents/allocator.png)  
