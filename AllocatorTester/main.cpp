// The test application currently works only under Windows.
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <Windows.h>
#include <Allocator.hpp>

static const int ACTION_COUNT = 1000*20000; // 20 million.
static const int ALLOCATE_THRESHOLD = 60;
static const int DEALLOCATE_THRESHOLD = 35;
static const int PASS_THRESHOLD = 5;
static const int MIN_OBJECT_SIZE = 8;
static const int MAX_OBJECT_SIZE = 256;
static const int MAX_THREAD_COUNT = 32;

// Interface that must be implemented by the supported allocators.
class AllocatorInterface {
public:
    virtual void* Allocate(int size) = 0;
    virtual void Deallocate(void* data) = 0;

    virtual ~AllocatorInterface() { }
};

// Implementation for an allocator using the default CRT allocator.
class NativeAllocator : public AllocatorInterface {
public:
    virtual void* Allocate(int size) {
        return malloc(size);
    }

    virtual void Deallocate(void* data) {
        free(data);
    }
};

// Implementatin for an allocator using the Parallel Allocator.
class ParallelAllocator : public AllocatorInterface {
public:
    Base::Allocator Allocator;

    virtual void* Allocate(int size) {
        return Allocator.Allocate(size);
    }

    virtual void Deallocate(void* data) {
        Allocator.Deallocate(data);
    }
};

// The actions that can be taken by a thread.
enum class ActionType {
    Allocate,
    Deallocate,
    Pass
};

// Stores pre-computed random numbers.
class RandomProvider {
private:
    std::vector<int> numbers;
    int position;

    int GetNextInt() {
        int value = numbers[position++] % 100;

        if(position == numbers.size()) {
            position = 0;
        }

        return value;
    }

public:
    RandomProvider() : numbers(), position(0) {
        // Add some random numbers. If more than this number
        // are requested some of the older numbers are reused.
        for(int i = 0; i < 10000; i++) {
            numbers.push_back(rand());
        }
    }

    ActionType GetNextAction() {
        int value = GetNextInt();
        
        if(value <= ALLOCATE_THRESHOLD) {
            return ActionType::Allocate;
        }
        else if((value - ALLOCATE_THRESHOLD) <= DEALLOCATE_THRESHOLD) {
            return ActionType::Deallocate;
        }
        else return ActionType::Pass;
    }

    int GetNextObjectSize() {
        int value = GetNextInt();
        return MIN_OBJECT_SIZE + (value % (MAX_OBJECT_SIZE - MIN_OBJECT_SIZE));
    }

    int GetNextInt(int maxValue) {
        return GetNextInt() % maxValue;
    }
};

// Stores the data 
class ThreadData {
public:
    std::vector<void*> Objects;
    std::vector<void*> PassedObjects;
    std::vector<int> FreeSlots;
    std::vector<ThreadData*> Threads;
    int deallocateObjects;
    AllocatorInterface* Allocator;
    RandomProvider RandomProvider;
    CRITICAL_SECTION PassedObjectsLock;
    
    void TouchData(void* data, int size) {
        // To make the pattern unique (and to be able to check 
        // if the next location has been overwritten) the magic constant
        // is combinedd with the first byte of the address.
        memset(data, 0xAB ^ (char)data, size);
    }

    bool VerifyData(void* data) {
        // Check if the object still begins with the expected pattern.
        // If not it means there is a bug in the allocator which allocates
        // overlapping locations.
        char expected_byte = 0xAB ^ (char)data;
        char expected_pattern[MIN_OBJECT_SIZE];
        memset(expected_pattern, expected_byte, MIN_OBJECT_SIZE);

        return memcmp(data, expected_pattern, MIN_OBJECT_SIZE) == 0;
        return true;
    }

    void InsertObject(void* object) {
        // Add the object to the list of free objects. If there are positions
        // that are not used by an object anymore prefer them.
        if(FreeSlots.size() > 0) {
            int slot = *(FreeSlots.end() - 1);
            FreeSlots.pop_back();
            Objects[slot] = object;
        }
        else Objects.push_back(object);
    }

    int SelectVictim() {
        // It's possible that the selected position doesn't contain
        // an object anymore, so try a few times. If still no used position
        // found select the first 
        int times = 0;
        int slot = -1;

        while(times < 8) {
            int victim = RandomProvider.GetNextInt(Objects.size());

            if(Objects[victim] != nullptr) {
                slot = victim;
                break;
            }
            else times++;
        }

        if(slot == -1) {
            // Search for the first available object.
            for(int i = 0; i < Objects.size(); i++) {
                if(Objects[i] != nullptr) {
                    slot = i;
                    break;
                }
            }
        }

        return slot;
    }

    void ResetSlot(int index) {
        Objects[index] = nullptr;
        FreeSlots.push_back(index);
        deallocateObjects++;
    }

public:
    ThreadData() : deallocateObjects(0) {
        InitializeCriticalSection(&PassedObjectsLock);
    }

    void Execute() {
        switch(RandomProvider.GetNextAction()) {
            case ActionType::Allocate: {
                // Allocate an object of a random size.
                int size = RandomProvider.GetNextObjectSize();
                void* object = Allocator->Allocate(size);

                if(object == 0) {
                    std::cout<<"Object could not be allocated!";
                    std::cout<<"Consider building in 64 bit mode.";
                    exit(-1);
                }

                TouchData(object, size);
                InsertObject(object);
                break;
            }
            case ActionType::Deallocate: {
                if(PassedObjects.size() > 0) {
                    EnterCriticalSection(&PassedObjectsLock);
                    void* object = *(PassedObjects.end() - 1);
                    PassedObjects.pop_back();
                    LeaveCriticalSection(&PassedObjectsLock);
                    
                    if(!VerifyData(object)) {
                        std::cout<<"Data corruption detected!\n";
                    }
                    
                    Allocator->Deallocate(object);
                    
                }
                if(Objects.size() > 0 && (deallocateObjects != Objects.size())) {
                    // Randomly select an object to be deallocated.
                    int victim = SelectVictim();

                    if(victim != -1) {
                        // Deallocate the object and insert the location
                        // in the list of unused positions to be used when allocating.
                        void* object = Objects[victim];

                        if(!VerifyData(object)) {
                            std::cout<<"Data corruption detected!\n";
                        }

                        Allocator->Deallocate(object);
                        ResetSlot(victim);
                    }
                }

                break;
            }
            case ActionType::Pass: {
                if(Threads.size() > 0 && Objects.size() > 0 &&
                   (deallocateObjects != Objects.size())) {
                    // Select an object to be passed to one of the other threads.
                    int victim = SelectVictim();

                    if(victim != -1) {
                        int otherThread = RandomProvider.GetNextInt(Threads.size());
                        ThreadData* otherThreadData = Threads[otherThread];
                    
                        void* object = Objects[victim];
                        ResetSlot(victim);

                        EnterCriticalSection(&otherThreadData->PassedObjectsLock);
                        otherThreadData->PassedObjects.push_back(object);
                        LeaveCriticalSection(&otherThreadData->PassedObjectsLock);
                    }
                }

                break;
            }
        }
    }

    void DeallocateAllObjects(std::vector<void*>& objects) {
        for(auto it = objects.begin(); it != objects.end(); ++it) {
            void* object = *it;
            if(object == nullptr) continue;

            if(!VerifyData(object)) {
                std::cout<<"Data corruption detected!\n";
            }
            
            Allocator->Deallocate(object);
        }
    }

    void ExecuteAllActions() {
        for(int i = 0; i < ACTION_COUNT; i++) {
            Execute();
        }

        DeallocateAllObjects(Objects);

        // Deallocate objects passed from other threads.
        EnterCriticalSection(&PassedObjectsLock);
        DeallocateAllObjects(PassedObjects);
        LeaveCriticalSection(&PassedObjectsLock);
    }
};


DWORD WINAPI ThreadCallback(LPVOID data) {
    ThreadData* threadData = reinterpret_cast<ThreadData*>(data);
    threadData->ExecuteAllActions();
    return 0;
}


int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout<<"Number of cores not specified";
    }

    int threadCount;
    sscanf(argv[1], "%d", &threadCount);

    srand(27);
    AllocatorInterface* allocator;

    if(argc > 2 && argv[2][0] == 'p') {
        allocator = new ParallelAllocator();
    }
    else allocator = new NativeAllocator();
    
    // Initialize the context for each thread.
    HANDLE threadHandles[MAX_THREAD_COUNT];
    ThreadData threadData[MAX_THREAD_COUNT];
    DWORD start = GetTickCount();

    for(int i = 0; i < threadCount; i++) {
        threadData[i].Allocator = allocator;

        for(int j = 0; j < threadCount; j++) {
            if(i != j) {
                threadData[i].Threads.push_back(&threadData[j]);
            }
        }
    }

    // Create the threads.
    for(int i = 0; i < threadCount; i++) {
        DWORD id;
        threadHandles[i] = CreateThread(NULL, 0, ThreadCallback, &threadData[i], 0, &id);
    }

    // Wait for all threads to finish.
    WaitForMultipleObjects(threadCount, threadHandles, TRUE, INFINITE);
    std::cout<<"Duration: "<<(GetTickCount() - start);
    return 0;
}
