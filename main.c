#include "allocator.c"


int main(__attribute__((unused)) int argc, __attribute__((unused)) const char* argv[]) {
    printf("---- Stack allocator ----\n");
    allocator_stack_t stack_alloc = allocator_stack_init(ALLOCATE_STACK(1024), 1024);
    allocator_t stack = { allocator_stack_proc, &stack_alloc };
    {
        byte_t* a = nax_allocate(stack, 10);
        byte_t* b = nax_allocate_aligned(stack, 155, 64);
        byte_t* c = nax_allocate(stack, 12);
        byte_t* d = nax_allocate_type(stack, int, 12);

        ASSERT(allocation_succeeded(a));
        ASSERT(allocation_succeeded(b));
        ASSERT(allocation_succeeded(c));
        ASSERT(allocation_succeeded(d));

        printf("%zu\n", nax_query_capacity(stack));
        printf("%zu\n", nax_query_alignment(stack));
        printf("%zu\n", nax_query_good_size(stack));
        printf("%zu\n", nax_query_used(stack));

        nax_free(stack, c);
        nax_free(stack, b);
        nax_free(stack, a);

        nax_free_all(stack);
    }


    printf("---- Freelist allocator ----\n");
    byte_t* result = nax_allocate(stack, 1024);
    ASSERT(allocation_succeeded(result));
    allocator_freelist_t freelist = freelist_init(result, 64, 1024/64);
    allocator_t freelist_allocator = (allocator_t) { allocator_freelist_proc, &freelist };
    {
        byte_t* x = nax_allocate(freelist_allocator, 64);
        byte_t* y = nax_allocate(freelist_allocator, 13);

        ASSERT(allocation_succeeded(x));
        ASSERT(allocation_succeeded(y));

        printf("%zu\n", nax_query_capacity(freelist_allocator));
        printf("%zu\n", nax_query_alignment(freelist_allocator));
        printf("%zu\n", nax_query_good_size(freelist_allocator));

        printf("%zu\n", nax_query_owns(freelist_allocator, x));
        printf("%zu\n", nax_query_owns(freelist_allocator, y));

        nax_free(freelist_allocator, x);
        nax_free(freelist_allocator, y);

        printf("%zu\n", nax_query_owns(freelist_allocator, x));
        printf("%zu\n", nax_query_owns(freelist_allocator, y));
    }
    nax_free_all(stack);


    printf("---- Fallback allocator ----\n");
    allocator_t primary   = stack;
    allocator_t secondary = (allocator_t) { allocator_malloc_proc, 0 };
    allocator_fallback_t fallback = { primary, secondary };
    allocator_t fallback_allocator = { allocator_fallback_proc, &fallback };
    {
        byte_t* x = nax_allocate(fallback_allocator, 1000);
        byte_t* y = nax_allocate(fallback_allocator, 1000);

        ASSERT(allocation_succeeded(x));
        ASSERT(allocation_succeeded(y));

        printf("%zu\n", nax_query_capacity(fallback_allocator));
        printf("%zu\n", nax_query_alignment(fallback_allocator));
        printf("%zu\n", nax_query_good_size(fallback_allocator));

        printf("%zu\n", nax_query_owns(fallback_allocator, x));
        printf("%zu\n", nax_query_owns(fallback_allocator, y));

        nax_free(fallback_allocator, x);
        nax_free(fallback_allocator, y);

        nax_free_all(fallback_allocator);
    }

    return 0;
}
