/* Contains a fallback allocator that can be used when the primary allocator
 * no longer can allocate.
 */
typedef struct {
    allocator_t primary;
    allocator_t secondary;
} allocator_fallback_t;


allocation_result_t fallback_allocate(allocator_fallback_t* allocator, word_t size) {
    byte_t* result = nax_allocate(allocator->primary, size);
    if (!allocation_succeeded(result)) {
        DO_ONCE(printf("Falling back to second allocator.\n"));
        result = nax_allocate(allocator->secondary, size);
        if (!allocation_succeeded(result)) {
            return make_allocation_error((allocation_status_t)(size_t) result);
        }
    }
    return make_allocation_result(result);
}


allocation_result_t fallback_allocate_aligned(allocator_fallback_t* allocator, word_t size, word_t alignment) {
    byte_t* result = nax_allocate_aligned(allocator->primary, size, alignment);
    if (!allocation_succeeded(result)) {
        DO_ONCE(printf("Falling back to second allocator.\n"));
        result = nax_allocate_aligned(allocator->secondary, size, alignment);
        if (!allocation_succeeded(result)) {
            return make_allocation_error((allocation_status_t)(size_t) result);
        }
    }
    return make_allocation_result(result);
}


allocation_result_t fallback_resize(allocator_fallback_t* allocator, byte_t* memory, word_t old_size, word_t new_size) {
    ASSERTF(0, "TODO");
    return make_allocation_error(ALLOCATION_STATUS_UNSUPPORTED_OPERATION);
}


allocation_result_t fallback_free(allocator_fallback_t* allocator, byte_t* memory) {
    size_t result = nax_free(allocator->primary, memory);
    if (!free_succeeded(result)) {
        result = nax_free(allocator->secondary, memory);
        if (!free_succeeded(result)) {
            return make_free_status((free_status_t) result);
        }
    }
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


allocation_result_t fallback_free_all(allocator_fallback_t* allocator) {
    // @TODO: Make sure these do not fail.
    nax_free_all(allocator->primary);
    nax_free_all(allocator->secondary);
    return make_free_status(FREE_STATUS_SUCCEEDED);
}


allocation_result_t fallback_owns(allocator_fallback_t* allocator, const byte_t* memory) {
    size_t query_1 = nax_query_owns(allocator->primary, memory);
    if (query_1) {
        return make_query_result(1);
    }

    size_t query_2 = nax_query_owns(allocator->secondary, memory);
    if (query_2) {
        return make_query_result(1);
    }

    if (query_1 == ALLOCATION_QUERY_UNSUPPORTED && query_2 == ALLOCATION_QUERY_UNSUPPORTED) {
        return make_query_result(ALLOCATION_QUERY_UNSUPPORTED);
    }

    return make_query_result(0);
}

allocation_result_t fallback_alignment(allocator_fallback_t* allocator) {
    size_t query_1 = nax_query_alignment(allocator->primary);
    size_t query_2 = nax_query_alignment(allocator->secondary);

    size_t minimum_alignment = (query_1 < query_2) ? query_1 : query_2;
    return make_query_result(minimum_alignment);
}

allocation_result_t fallback_good_size(allocator_fallback_t* allocator) {
    size_t query_1 = nax_query_good_size(allocator->primary);
    size_t query_2 = nax_query_good_size(allocator->secondary);

    size_t minimum_good_size = (query_1 < query_2) ? query_1 : query_2;
    return make_query_result(minimum_good_size);
}

allocation_result_t fallback_capacity(allocator_fallback_t* allocator) {
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

allocation_result_t fallback_used(allocator_fallback_t* allocator) {
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



allocation_result_t allocator_fallback_proc(void* allocator_raw, allocation_arguments_t arguments) {
    allocator_fallback_t* allocator = (allocator_fallback_t*) allocator_raw;
    switch (arguments.mode) {
        case ALLOCATE:          return fallback_allocate(allocator, arguments.allocate.size);
        case ALLOCATE_ALIGNED:  return fallback_allocate_aligned(allocator, arguments.allocate_aligned.size, arguments.allocate_aligned.alignment);
        case ALLOCATE_ALL:      return make_allocation_error(ALLOCATION_STATUS_UNSUPPORTED_OPERATION);
        case RESIZE:            return fallback_resize(allocator, arguments.resize.memory, arguments.resize.old_size, arguments.resize.new_size);
        case FREE:              return fallback_free(allocator, arguments.free.memory);
        case FREE_ALL:          return fallback_free_all(allocator);

        case QUERY_USED:        return fallback_used(allocator);
        case QUERY_OWNS:        return fallback_owns(allocator, arguments.owns.memory);
        case QUERY_CAPACITY:    return fallback_capacity(allocator);
        case QUERY_ALIGNMENT:   return fallback_alignment(allocator);
        case QUERY_GOOD_SIZE:   return fallback_good_size(allocator);
    }
}
