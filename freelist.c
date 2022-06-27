typedef struct {
    u32 one_past_next;
} freelist_node_t;


typedef struct {
    byte_t* m_memory;
    u32     m_first_free;
    u32     m_block_size;
    u32     m_count;
    u32     m_used;
} allocator_freelist_t;


int freelist_owns(allocator_freelist_t* allocator, const byte_t* memory);
size_t freelist_capacity(allocator_freelist_t* allocator);


allocator_freelist_t freelist_init(byte_t* memory, u32 block_size, u32 count) {
    ASSERTF(block_size % sizeof(freelist_node_t) == 0 , "Must have block size to be a multiple of sizeof(freelist_node_t)!");
    allocator_freelist_t freelist = {
            .m_memory = memory,
            .m_first_free = 0,
            .m_block_size = block_size,
            .m_count = count,
            .m_used  = 0,
    };
    memset(freelist.m_memory, 0, freelist_capacity(&freelist));
    return freelist;
}


allocation_result_t freelist_allocate(allocator_freelist_t* allocator, word_t size) {
    ASSERTF((u32) size <= allocator->m_block_size, "Allocating more than block size!");

    if (allocator->m_first_free >= freelist_capacity(allocator)) {   // Out of memory.
        return make_allocation_error(ALLOCATION_STATUS_OUT_OF_MEMORY);
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
    freelist_node_t* element = (freelist_node_t*) &allocator->m_memory[allocator->m_first_free * allocator->m_block_size];
    ASSERTF(align_address((size_t) element, ALIGN_OF(freelist_node_t)) == (size_t) element, "Element must be aligned to freelist_node_t");
#pragma clang diagnostic pop

    allocator->m_first_free = (element->one_past_next == 0) ? allocator->m_first_free + 1 : element->one_past_next - 1;
    allocator->m_used += 1;
    return make_allocation_result((byte_t*) element);
}


allocation_result_t freelist_resize(allocator_freelist_t* allocator, byte_t* memory, word_t old_size, word_t new_size)  {
    ASSERTF(0, "TODO");
    return make_allocation_error(ALLOCATION_STATUS_SUCCEEDED);
}


allocation_result_t freelist_free(allocator_freelist_t* allocator, byte_t* memory)  {
    ASSERTF(freelist_owns(allocator, memory), "Allocator does not own the memory!");
    ASSERTF(allocator->m_first_free <= freelist_capacity(allocator), "Allocator was empty!");

    u32 offset = (u32) ((byte_t*)memory - (byte_t*)allocator->m_memory);
    ASSERTF(offset % allocator->m_block_size == 0, "Invalid offset of pointer!");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
    freelist_node_t* element = (freelist_node_t*) &allocator->m_memory[offset];
    ASSERTF(align_address((size_t) element, ALIGN_OF(freelist_node_t)) == (size_t) element, "Element must be aligned to freelist_node_t");
#pragma clang diagnostic pop

    element->one_past_next = allocator->m_first_free + 1;
    allocator->m_first_free = offset / allocator->m_block_size;
    allocator->m_used -= 1;
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


allocation_result_t freelist_free_all(allocator_freelist_t* allocator)  {
    allocator->m_first_free = 0;
    allocator->m_used = 0;
    memset(allocator->m_memory, 0, freelist_capacity(allocator));
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


int freelist_owns(allocator_freelist_t* allocator, const byte_t* memory) {
    int owns = allocator->m_memory <= memory && memory <= allocator->m_memory + allocator->m_count * allocator->m_block_size;
    return owns;
}


size_t freelist_capacity(allocator_freelist_t* allocator) {
    return allocator->m_block_size * allocator->m_count;
}

size_t freelist_used(allocator_freelist_t* allocator) {
    return allocator->m_block_size * allocator->m_used;
}


allocation_result_t allocator_freelist_proc(void* allocator_raw, allocation_arguments_t arguments) {
    allocator_freelist_t* allocator = (allocator_freelist_t*) allocator_raw;
    switch (arguments.mode) {
        case ALLOCATE:          return freelist_allocate(allocator, arguments.allocate.size);
        case ALLOCATE_ALIGNED:
            ASSERTF((u32) arguments.allocate_aligned.alignment == allocator->m_block_size, "Can only align at block size");
            return freelist_allocate(allocator, arguments.allocate_aligned.size);
        case RESIZE:            return freelist_resize(allocator, arguments.resize.memory, arguments.resize.old_size, arguments.resize.new_size);
        case FREE:              return freelist_free(allocator, arguments.free.memory);
        case FREE_ALL:          return freelist_free_all(allocator);
        case QUERY_USED:        return make_query_result(freelist_used(allocator));
        case QUERY_OWNS:        return make_query_result((size_t) freelist_owns(allocator, arguments.owns.memory));
        case QUERY_CAPACITY:    return make_query_result(freelist_capacity(allocator));
        case QUERY_ALIGNMENT:
        case QUERY_GOOD_SIZE:   return make_query_result(allocator->m_block_size);
        case ALLOCATE_ALL:      return make_allocation_error(ALLOCATION_STATUS_UNSUPPORTED_OPERATION);
    }
}
