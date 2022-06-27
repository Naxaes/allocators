typedef struct {
    byte_t* m_memory;
    u32     m_pointer;
    u32     m_capacity;
} allocator_stack_t;


int stack_owns(allocator_stack_t* allocator, const byte_t* memory);


allocator_stack_t allocator_stack_init(byte_t* memory, u32 capacity) {
    return (allocator_stack_t) {
            .m_memory   = memory,
            .m_pointer  = 0,
            .m_capacity = capacity
    };
}


allocation_result_t stack_allocate(allocator_stack_t* allocator, word_t size) {
    if (allocator->m_pointer + (size_t) size > allocator->m_capacity) {
        return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
    } else {
        byte_t* memory = allocator->m_memory + allocator->m_pointer;
        allocator->m_pointer += (u32) size;
        return make_allocation_result(memory);
    }
}


allocation_result_t stack_allocate_aligned(allocator_stack_t* allocator, word_t size, word_t alignment) {
    size_t aligned_address   = align_address((size_t) (allocator->m_memory + allocator->m_pointer), (size_t) alignment);
    size_t alignment_padding = aligned_address - (size_t) (allocator->m_memory + allocator->m_pointer);

    if (allocator->m_pointer + (size_t) size + alignment_padding > allocator->m_capacity) {
        return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
    } else {
        byte_t* memory = (byte_t*) aligned_address;
        allocator->m_pointer += (u32) size + alignment_padding;
        return make_allocation_result(memory);
    }
}

allocation_result_t stack_allocate_all(allocator_stack_t* allocator) {
    if (allocator->m_pointer == allocator->m_capacity) {
        return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
    } else {
        byte_t* memory = allocator->m_memory + allocator->m_pointer;
        allocator->m_pointer = allocator->m_capacity;
        return make_allocation_result(memory);
    }
}


allocation_result_t stack_resize(allocator_stack_t* allocator, byte_t* old_memory, word_t old_size, word_t new_size) {
    if (old_memory == 0) {
        return stack_allocate(allocator, new_size);
    }

    ASSERT(stack_owns(allocator, old_memory));
    ASSERT(old_size == ((allocator->m_memory + allocator->m_pointer) - (byte_t *) old_memory));

    allocator->m_pointer -= (u32)old_size;
    if (allocator->m_pointer + (u32)new_size > allocator->m_capacity) {
        return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
    } else {
        byte_t* memory = allocator->m_memory + allocator->m_pointer;
        allocator->m_pointer += (u32)new_size;
        return make_allocation_result(memory);
    }
}


allocation_result_t stack_free(allocator_stack_t* allocator, byte_t* memory) {
    if (!stack_owns(allocator, memory))
        return make_free_status(FREE_STATUS_CALLED_ON_NON_OWNED_MEMORY);

    long size = (allocator->m_memory + allocator->m_pointer) - (byte_t *) memory;
    if (size <= 0)
        return make_free_status(FREE_STATUS_CALLED_ON_NON_OWNED_MEMORY);  // @TODO: Make better error message.

    allocator->m_pointer -= size;

    DEBUG_BLOCK(memset(allocator->m_memory + allocator->m_pointer, 0xCC, (size_t)size));
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


int stack_owns(allocator_stack_t* allocator, const byte_t* memory) {
    int is_owned_by_allocator = allocator->m_memory <= memory && memory <= allocator->m_memory + allocator->m_pointer;
    DEBUG_BLOCK(
        if (!is_owned_by_allocator && allocator->m_memory <= memory && memory <= allocator->m_memory + allocator->m_capacity)
            ASSERTF(0, "The memory has already been freed!")
    );
    return is_owned_by_allocator;
}


allocation_result_t stack_free_all(allocator_stack_t* allocator) {
    allocator->m_pointer = 0;
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


size_t stack_used(allocator_stack_t* allocator) {
    return allocator->m_pointer;
}

size_t stack_capacity(allocator_stack_t* allocator) {
    return allocator->m_capacity;
}


allocation_result_t allocator_stack_proc(void* allocator_raw, allocation_arguments_t arguments) {
    allocator_stack_t* allocator = (allocator_stack_t*) allocator_raw;
    switch (arguments.mode) {
        case ALLOCATE:          return stack_allocate(allocator, arguments.allocate.size);
        case ALLOCATE_ALIGNED:  return stack_allocate_aligned(allocator, arguments.allocate_aligned.size, arguments.allocate_aligned.alignment);
        case ALLOCATE_ALL:      return stack_allocate_all(allocator);
        case RESIZE:            return stack_resize(allocator, arguments.resize.memory, arguments.resize.old_size, arguments.resize.old_size);
        case FREE:              return stack_free(allocator, arguments.free.memory);
        case FREE_ALL:          return stack_free_all(allocator);
        case QUERY_USED:        return make_query_result(stack_used(allocator));
        case QUERY_OWNS:        return make_query_result((size_t) stack_owns(allocator, arguments.owns.memory));
        case QUERY_CAPACITY:    return make_query_result(stack_capacity(allocator));
        case QUERY_ALIGNMENT:
        case QUERY_GOOD_SIZE:   return make_query_result(1);
    }
}
