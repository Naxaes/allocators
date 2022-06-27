typedef struct {
    allocator_t primary;
    allocator_t secondary;
    word_t threshold;
} allocator_segregator_t;


allocation_result_t segregator_allocate(allocator_segregator_t* allocator, word_t size) {
    byte_t* memory = 0;
    if (size <= allocator->threshold) {
        memory = nax_allocate(allocator->primary, size);
        if (!allocation_succeeded(memory))
            return make_allocation_error((allocation_status_t)(size_t) memory);
    } else {
        memory = nax_allocate(allocator->secondary, size);
        if (!allocation_succeeded(memory))
            return make_allocation_error((allocation_status_t)(size_t) memory);
    }
    return make_allocation_result(memory);
}


allocation_result_t segregator_allocate_aligned(allocator_segregator_t* allocator, word_t size, word_t alignment) {
    // @TODO: What to do when alignment increases the size above the threshold?
    byte_t* memory = 0;
    if (size <= allocator->threshold) {
        memory = nax_allocate_aligned(allocator->primary, size, alignment);
        if (!allocation_succeeded(memory))
            return make_allocation_error((allocation_status_t)(size_t) memory);
    } else {
        memory = nax_allocate_aligned(allocator->secondary, size, alignment);
        if (!allocation_succeeded(memory))
            return make_allocation_error((allocation_status_t)(size_t) memory);
    }
    return make_allocation_result(memory);
}


allocation_result_t segregator_resize(allocator_segregator_t* allocator, byte_t* memory, word_t old_size, word_t new_size) {
    ASSERTF(0, "TODO");
    return (allocation_result_t) { 0 };
}


allocation_result_t segregator_free(allocator_segregator_t* allocator, byte_t* memory) {
    ASSERTF(0, "TODO");
    return (allocation_result_t) { 0 };
}


allocation_result_t segregator_free_all(allocator_segregator_t* allocator) {
    // @TODO: Make sure these do not fail.
    nax_free_all(allocator->primary);
    nax_free_all(allocator->secondary);
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


allocation_result_t segregator_owns(allocator_segregator_t* allocator, const byte_t* memory) {
    size_t query_1 = nax_query_owns(allocator->primary, memory);
    if (query_1 == 1) {
        return make_query_result(1);
    }

    size_t query_2 = nax_query_owns(allocator->secondary, memory);
    if (query_2 == 1) {
        return make_query_result(1);
    }

    if (query_1 == ALLOCATION_QUERY_UNSUPPORTED && query_2 == ALLOCATION_QUERY_UNSUPPORTED) {
        return make_query_result(ALLOCATION_QUERY_UNSUPPORTED);
    }

    return make_query_result(0);
}

allocation_result_t segregator_alignment(allocator_segregator_t* allocator) {
    size_t query_1 = nax_query_alignment(allocator->primary);
    size_t query_2 = nax_query_alignment(allocator->secondary);

    size_t minimum_alignment = (query_1 < query_2) ? query_1 : query_2;
    return make_query_result(minimum_alignment);
}

allocation_result_t segregator_good_size(allocator_segregator_t* allocator) {
    size_t query_1 = nax_query_good_size(allocator->primary);
    size_t query_2 = nax_query_good_size(allocator->secondary);

    size_t minimum_good_size = (query_1 < query_2) ? query_1 : query_2;
    return make_query_result(minimum_good_size);
}

allocation_result_t segregator_capacity(allocator_segregator_t* allocator) {
    size_t query_1 = nax_query_capacity(allocator->primary);
    size_t query_2 = nax_query_capacity(allocator->secondary);

    if (query_1 != ALLOCATION_QUERY_UNSUPPORTED && query_2 != ALLOCATION_QUERY_UNSUPPORTED) {
        return make_query_result(query_1 + query_2);
    }
    if (query_1 != ALLOCATION_QUERY_UNSUPPORTED) {
        return make_query_result(query_1);
    }
    return make_query_result(query_2);
}

allocation_result_t segregator_used(allocator_segregator_t* allocator) {
    size_t query_1 = nax_query_used(allocator->primary);
    size_t query_2 = nax_query_used(allocator->secondary);

    if (query_1 != ALLOCATION_QUERY_UNSUPPORTED && query_2 != ALLOCATION_QUERY_UNSUPPORTED) {
        return make_query_result(query_1 + query_2);
    }
    if (query_1 != ALLOCATION_QUERY_UNSUPPORTED) {
        return make_query_result(query_1);
    }
    return make_query_result(query_2);
}



allocation_result_t allocator_segregator_proc(void* allocator_raw, allocation_arguments_t arguments) {
    allocator_segregator_t* allocator = (allocator_segregator_t*) allocator_raw;
    switch (arguments.mode) {
        case ALLOCATE:          return segregator_allocate(allocator, arguments.allocate.size);
        case ALLOCATE_ALIGNED:  return segregator_allocate_aligned(allocator, arguments.allocate_aligned.size, arguments.allocate_aligned.alignment);
        case ALLOCATE_ALL:      return make_allocation_error(ALLOCATION_STATUS_UNSUPPORTED_OPERATION);
        case RESIZE:            return segregator_resize(allocator, arguments.resize.memory, arguments.resize.old_size, arguments.resize.new_size);
        case FREE:              return segregator_free(allocator, arguments.free.memory);
        case FREE_ALL:          return segregator_free_all(allocator);

        case QUERY_USED:        return segregator_used(allocator);
        case QUERY_OWNS:        return segregator_owns(allocator, arguments.owns.memory);
        case QUERY_CAPACITY:    return segregator_capacity(allocator);
        case QUERY_ALIGNMENT:   return segregator_alignment(allocator);
        case QUERY_GOOD_SIZE:   return segregator_good_size(allocator);
    }
}
