// TODO: 
// Create some threads
// Each thread does randomly some actions:
//    - allocate object
//    - deallocate object
//    - pass object to other thread (seldom)
// Each thread does N operations, then deallocates all remaining objects.
// Main thread waits for all threads to complete and reports duration.
// Switch to change between malloc and the parallel allocator.
//    void* Allocate(size) {
//       #ifdef BUILTIN_ALLOC
//           return malloc(size);
//       #else ...