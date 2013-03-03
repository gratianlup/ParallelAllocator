Parallel Allocator
===================

A memory allocator optimized for high concurrency and low memory consumption on modern multi-core and multi-processor systems.  
It includes special support for NUMA systems (allocating memory from the nearest NUMA node, for example).

It is about 60% faster than the allocator included with Microsoft Visual C++ and uses less than half the memory for small objects  
(< 256 bytes), leading to better cache utilization; this also makes it ideal for applications that allocate a huge number of small objects.  
In many tests it is about 10% faster than the allocator included with Intel Thread Building Blocks.  

It is implemented in C++ with a small amount of x86 Assembly and was recently converted to C++11.  
Access to low-level synchronization and bit-manipulation instructions is done through compiler intrinsics.  

### Architecture overview:  

The arhitecture and the implementaion details are described in the following document:  
**[Download arhitecture and the implementaion details (PDF)](http://www.gratianlup.com/documents/parallel_allocator.pdf)**  
  
![SmartFlip screenshot](http://www.gratianlup.com/documents/allocator_summary.png)  

### Implementation details

The following documents present some of the implementation details:  
**[Download internal data structures details (PDF)](http://www.gratianlup.com/documents/allocator.pdf)**  
**[Download bin location size details (PDF)](http://www.gratianlup.com/documents/allocator_bins.pdf)**  

![SmartFlip screenshot](http://www.gratianlup.com/documents/allocator.png)  
