/*
The allocators are divided into 3 categories:
    1. Allocators.  Things that fetches memory.
    2. Strategies.  Things that manage memory.
    3. Compositors. Things that combine allocators and strategies.

There are 4 allocators:
    1. System allocator - Asks the OS for dynamic memory.
    2. Stack allocator  - Use the stack. This is special since it is also a strategy.
    3. Null allocator   - Always return null.
    4. Panic allocator  - Always crashes.

After we got memory, there are different strategies of handling that memory:
    1. Bump/Arena - Useful for temporary allocations.
        * O(1) allocations.
        * Can only free latest allocation.
    2. Free list - Useful for Pareto-distributed allocations.
        * O(1) allocation.
        * O(1) free.
        * Fixed size allocations.
    3. Pool -
        * Fixed size allocations.
        * Can only free in pools.
    4. Heap
        * Arbitrary sized allcations.
        * O(log n) allocation.
    5. Buddy

These can be combined with:
    1. Fallback   - Allocates with a primary allocator and fallsback to a secondaty when the primary fails.
    2. Segregator - Allocate with a primary allocator if a certain threshold is met, otherwise allocate with a secondary->
    4. Cascading  - Allocates new allocators when the previous runs out.
    5. Bucketizer

*/

// https://accu.org/conf-docs/PDFs_2008/Alexandrescu-memory-allocation.screen.pdf
// https://www.youtube.com/watch?v=LIb3L4vKZ7U

// Allocator API:
//     static constexpr u32 alignment;
//     static constexpr u32 good_size;
//     Box allocate(size_t);
//     Box allocate_all();
//     int  expand(Box&, size_t);
//     void  reallocate(Box&, size_t);
//     int  owns(Box);
//     void  deallocate(Box);
//     void  deallocate_all();
#include "preamble.h"
#include <stdlib.h>
#include <string.h>

#define ALIGN_OF(type) offsetof(struct { char c; type member; }, member)
#define DO_ONCE(x) do { static int first_time = 1; if (first_time) { x; first_time = 0; } } while (0)


int is_power_of_two(u64 value) {
    return value && ((value & (value - 1)) == 0);
}

size_t align_address(size_t address, size_t alignment) {
    ASSERT(is_power_of_two(alignment));
    size_t mask = alignment - 1;
    size_t aligned_address = (address + mask) & ~mask;
    return aligned_address;
}

word_t round_to_aligned(word_t size, word_t alignment) {
    ASSERT(size > 0);
    return size + (alignment - 1) & -alignment;
}

word_t alignment_padding(word_t size, word_t alignment) {
    ASSERT(size > 0);
    return (alignment - 1) & -alignment;
}



//https://github.com/odin-lang/Odin/blob/9349dfba8fec53f52f77a0c8928e115ec93ff447/core/runtime/core_builtin.odin
typedef struct {
    const char* file;
    const char* function;
    const int   line;
} source_location_t;


typedef enum {
    ALLOCATION_STATUS_SUCCEEDED = 0,           // When no memory is allocated but it "succeeds", like a null_allocator or log_allocator.
    ALLOCATION_STATUS_OUT_OF_MEMORY,
    ALLOCATION_STATUS_UNSUPPORTED_OPERATION,   // Using an operation an allocator doesn't have, like ALLOCATE_ALL on system allocator.
    ALLOCATION_STATUS_NON_OWNED_MEMORY,
    ALLOCATION_STATUS_COUNT,                   // The amount of error values.
} allocation_status_t;


typedef enum {
    FREE_STATUS_SUCCEEDED = 0,
    FREE_STATUS_CALLED_ON_NON_OWNED_MEMORY,
    FREE_STATUS_UNSUPPORTED_OPERATION,        // Using an operation an allocator doesn't have, like FREE_ALL on system allocator.
    FREE_STATUS_COUNT,
} free_status_t;


// @NOTE: Since many queries expect a small number including 0, this needs to be defined to differentiate
//        an invalid query from a valid.
const size_t ALLOCATION_QUERY_UNSUPPORTED = (size_t) - 1;


typedef enum {
    // Allocates `size` bytes with default alignment.
    ALLOCATE,

    // Allocates `size` bytes with aligned by `alignment`.
    ALLOCATE_ALIGNED,

    // Allocates all available memory.
    ALLOCATE_ALL,

    // Resizes the allocation to `new_size` by either shrinking or expanding.
    // May reallocate if expanding isn't possible, in which all data will be
    // copied to a new memory location, keeping the same alignment.
    RESIZE,

    // Frees memory at a certain address, if possible.
    FREE,

    // Frees all memory of an allocator, if possible.
    FREE_ALL,

    // Asks an allocator how much memory it has used.
    QUERY_USED,

    // Asks an allocator if it owns a memory.
    QUERY_OWNS,

    // Asks an allocator how much memory it can allocate.
    QUERY_CAPACITY,

    // Asks an allocator what its alignment is.
    QUERY_ALIGNMENT,

    // Asks an allocator what its smallest manageable allocation size is. Many allocators
    // will align or pad, so this value tells the user for which size most memory is utilized.
    QUERY_GOOD_SIZE,
} allocation_mode_t;


// @NOTE: This assumes that the first page is reserved. On embedded platforms where
//        this is not true, simply change `union` to `struct`.
// @NOTE: Currently, memory can be 0 or above 4096, but not 0xFFFFFFFFFFFFFFFF.
//        An error code or query result can be anything less than 4096.
typedef union {
    byte_t* memory;
    size_t  result;
} allocation_result_t;

allocation_result_t make_allocation_result(byte_t* memory) {
    size_t address = (size_t) memory;
    ASSERTF(address > 4096 && address != ALLOCATION_QUERY_UNSUPPORTED, "Invalid memory address %zu.", address);
    return (allocation_result_t) { .memory=memory };
}

allocation_result_t make_free_status(free_status_t status) {
    ASSERTF(status < FREE_STATUS_COUNT, "Invalid encoding %zu", (size_t) status);
    return (allocation_result_t) { .result=status };
}

allocation_result_t make_allocation_error(allocation_status_t status) {
    ASSERTF(status < ALLOCATION_STATUS_COUNT, "Invalid encoding %zu", (size_t) status);
    return (allocation_result_t) { .result=status };
}

allocation_result_t make_query_result(size_t result) {
    return (allocation_result_t) { .result=result };
}

// @NOTE: Calling this with an allocation_result_t from a query will yield invalid results.
int allocation_succeeded(const byte_t* allocation) {
    size_t address = (size_t) allocation;
    int is_ok = address == ALLOCATION_STATUS_SUCCEEDED || address >= ALLOCATION_STATUS_COUNT;
    return is_ok;
}

int free_succeeded(size_t status) {
    int is_ok = status == FREE_STATUS_SUCCEEDED;
    return is_ok;
}


typedef struct {
    allocation_mode_t mode;

    union {
        struct {
            word_t size;
        } allocate;

        struct {
            word_t size;
            word_t alignment;
        } allocate_aligned;

        struct {
            byte_t* memory;
            word_t  new_size;
            word_t  old_size;
        } resize;

        struct {
            byte_t* memory;
        } free;

        struct {
            const byte_t* memory;
        } owns;
    };
} allocation_arguments_t;

typedef allocation_result_t (*allocator_fn)(void* allocator, allocation_arguments_t arguments);

/// Polymorphic allocator.
typedef struct {
    allocator_fn procedure;
    void* data;
} allocator_t;



static inline allocation_result_t allocation_proxy(allocator_t allocator, allocation_arguments_t arguments, source_location_t location) {
    // @TODO: Add pre-post function calls.
    allocation_result_t result = allocator.procedure(allocator.data, arguments);
    return result;
}


#define nax_allocate_type(allocator, type_, count_)          allocation_proxy(allocator, (allocation_arguments_t) { .mode=ALLOCATE_ALIGNED, .allocate_aligned={ .size=count_ * sizeof(type_), .alignment=ALIGN_OF(type_) }}, (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).memory

#define nax_allocate(allocator, size_)                       allocation_proxy(allocator, (allocation_arguments_t) { .mode=ALLOCATE,         .allocate={ .size=size_ }},                                                      (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).memory
#define nax_allocate_aligned(allocator, size_, alignment_)   allocation_proxy(allocator, (allocation_arguments_t) { .mode=ALLOCATE_ALIGNED, .allocate_aligned={ .size=size_, .alignment=alignment_ }},                       (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).memory
#define nax_allocate_all(allocator)                          allocation_proxy(allocator, (allocation_arguments_t) { .mode=ALLOCATE_ALL,      },                                                                              (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).memory
#define nax_resize(allocator, memory_, new_size_, old_size_) allocation_proxy(allocator, (allocation_arguments_t) { .mode=RESIZE,           .resize={ .memory=memory_, .new_size=new_size_, .old_size=old_size_ }},          (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).memory
#define nax_free(allocator, memory_)                         allocation_proxy(allocator, (allocation_arguments_t) { .mode=FREE,             .free={ .memory=memory_ }},                                                      (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result
#define nax_free_all(allocator)                              allocation_proxy(allocator, (allocation_arguments_t) { .mode=FREE_ALL,          },                                                                              (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result
#define nax_query_owns(allocator, memory_)                   allocation_proxy(allocator, (allocation_arguments_t) { .mode=QUERY_OWNS,       .owns={ .memory=memory_ }},                                                      (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result
#define nax_query_used(allocator)                            allocation_proxy(allocator, (allocation_arguments_t) { .mode=QUERY_USED,        },                                                                              (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result
#define nax_query_capacity(allocator)                        allocation_proxy(allocator, (allocation_arguments_t) { .mode=QUERY_CAPACITY,    },                                                                              (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result
#define nax_query_alignment(allocator)                       allocation_proxy(allocator, (allocation_arguments_t) { .mode=QUERY_ALIGNMENT,   },                                                                              (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result
#define nax_query_good_size(allocator)                       allocation_proxy(allocator, (allocation_arguments_t) { .mode=QUERY_GOOD_SIZE,   },                                                                              (source_location_t) { .file=__FILE__, .function=__FUNCTION__, .line=__LINE__ }).result






/* ---- MEMORY ALLOCATORS ---- */
#define ALLOCATE_STACK(capacity) ((byte_t*) memset(alloca(capacity), 0xCC, capacity))

allocation_result_t allocator_null_proc(void* allocator, allocation_arguments_t arguments) {
    // @NOTE: Null allocator needs to bypass the checks in `make_allocation_result`, as 0 is only a valid
    //        address for this allocator. Therefore, `allocation_result_t` is assigned directly.
    switch (arguments.mode) {
        case ALLOCATE:         return (arguments.allocate.size == 0)         ? (allocation_result_t) { .memory=0 } : make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
        case ALLOCATE_ALIGNED: return (arguments.allocate_aligned.size == 0) ? (allocation_result_t) { .memory=0 } : make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
        case ALLOCATE_ALL:     return (allocation_result_t) { .memory=0 };
        case RESIZE:           return (arguments.resize.new_size == 0) ? (allocation_result_t) { .memory=0 } : make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
        case FREE:             return (arguments.free.memory     == 0) ? make_free_status(FREE_STATUS_SUCCEEDED) : make_free_status(FREE_STATUS_CALLED_ON_NON_OWNED_MEMORY);
        case FREE_ALL:         return make_free_status(FREE_STATUS_SUCCEEDED);
        case QUERY_OWNS:       return make_query_result(arguments.owns.memory == 0);

        // @NOTE: Unsupported.
        case QUERY_CAPACITY:
        case QUERY_ALIGNMENT:
        case QUERY_GOOD_SIZE:
        case QUERY_USED:
            return make_query_result(ALLOCATION_QUERY_UNSUPPORTED);
    }
}

allocation_result_t allocator_panic_proc(__attribute__((unused)) void* allocator, __attribute__((unused)) allocation_arguments_t arguments) {
    ASSERTF(0, "PANIC!");
    return (allocation_result_t) { 0 };
}

allocation_result_t allocator_malloc_proc(__attribute__((unused)) void* allocator, allocation_arguments_t arguments) {
    switch (arguments.mode) {
        case ALLOCATE: {
            byte_t* memory = (byte_t*) malloc((size_t) arguments.allocate.size);
            if (memory != 0) {
                memset(memory, 0xCC, (size_t) arguments.allocate.size);
                return make_allocation_result(memory);
            } else {
                return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
            }
        }
        case ALLOCATE_ALIGNED: {
            word_t  aligned_size = round_to_aligned(arguments.allocate_aligned.size, arguments.allocate_aligned.alignment);
            byte_t* memory = (byte_t*) malloc((size_t) aligned_size);
            if (memory != 0) {
                memset(memory, 0xCC, (size_t) aligned_size);
                return make_allocation_result(memory);
            } else {
                return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
            }
        }
        case RESIZE: {
            word_t  aligned_size = round_to_aligned(arguments.resize.new_size, 8);  // @TODO: Get alignment from memory address.
            byte_t* memory = (byte_t*) realloc(arguments.resize.memory, (size_t) aligned_size);
            if (memory != 0) {
                memset(memory, 0xCC, (size_t) aligned_size);
                return make_allocation_result(memory);
            } else {
                return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
            }
        }
        case FREE: {
            free(arguments.free.memory);
            return make_free_status(FREE_STATUS_SUCCEEDED);
        }

        // @NOTE: Unsupported.
        case ALLOCATE_ALL:
        case FREE_ALL:
            return make_allocation_error(ALLOCATION_STATUS_UNSUPPORTED_OPERATION);

        case QUERY_OWNS:
        case QUERY_CAPACITY:
        case QUERY_ALIGNMENT:
        case QUERY_GOOD_SIZE:
        case QUERY_USED:
            return make_query_result(ALLOCATION_QUERY_UNSUPPORTED);
    }
}



/* ---- ALLOCATORS ---- */
static allocator_t allocator_null    = { .procedure=allocator_null_proc,   .data=0 };
static allocator_t allocator_panic   = { .procedure=allocator_panic_proc,  .data=0 };
static allocator_t allocator_malloc  = { .procedure=allocator_malloc_proc, .data=0 };


/* ---- ALLOCATORS STRATEGIES ---- */
#include "stack.c"
#include "freelist.c"


/* ---- ALLOCATORS COMPOSITORS ---- */
#include "fallback.c"
#include "segregator.c"



