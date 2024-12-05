Parallel Allocator
===================

A memory allocator optimized for high concurrency and low memory consumption on modern multi-core and multi-processor systems. It includes special support for NUMA systems (allocating memory from the nearest NUMA node, for example).

It is about 60% faster than the allocator included with Microsoft Visual C++ and uses less than half the memory for small objects (< 256 bytes), leading to better cache utilization; this also makes it ideal for applications that allocate a huge number of small objects.  
In many tests it is about 10% faster than the allocator included with Intel Thread Building Blocks.  

It is implemented in C++ with a small amount of x86 Assembly and was recently converted to C++11.  
Access to low-level synchronization and bit-manipulation instructions is done through compiler intrinsics.  

### Architecture overview:  

The architecture and the implementation details are described in the following document:  
[Architecture and implementation details (PDF)](https://github.com/user-attachments/files/18029355/memory_allocator.pdf)

Overview of main data structures:
![Screenshot 2024-12-05 122637](https://github.com/user-attachments/assets/94c9339f-a029-4bab-a37a-a66b9313b4bc)
