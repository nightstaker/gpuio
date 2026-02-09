/**
 * @file memory.c
 * @brief Memory management implementation
 * @version 1.0.0
 */

#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* ============================================================================
 * Host Memory Allocation
 * ============================================================================ */

gpuio_error_t gpuio_malloc(gpuio_context_t ctx, size_t size, void** ptr) {
    if (!ctx || !ptr) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    *ptr = malloc(size);
    if (!*ptr) {
        return GPUIO_ERROR_NOMEM;
    }
    
    LOG(ctx, GPUIO_LOG_DEBUG, "Allocated %zu bytes at %p", size, *ptr);
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_malloc_pinned(gpuio_context_t ctx, size_t size, void** ptr) {
    if (!ctx || !ptr) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    /* Try vendor-specific pinned allocation first */
    if (current_vendor_ops && current_vendor_ops->malloc_pinned) {
        int ret = current_vendor_ops->malloc_pinned(ctx, size, ptr);
        if (ret == 0) {
            LOG(ctx, GPUIO_LOG_DEBUG, "Allocated %zu bytes pinned memory at %p", 
                size, *ptr);
            return GPUIO_SUCCESS;
        }
    }
    
    /* Fallback to mmap with huge pages hint */
    *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (*ptr == MAP_FAILED) {
        *ptr = NULL;
        return GPUIO_ERROR_NOMEM;
    }
    
    /* Advise for sequential access */
    madvise(*ptr, size, MADV_SEQUENTIAL);
    
    LOG(ctx, GPUIO_LOG_DEBUG, "Allocated %zu bytes host memory at %p", size, *ptr);
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_malloc_device(gpuio_context_t ctx, size_t size, void** ptr) {
    if (!ctx || !ptr) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (current_vendor_ops && current_vendor_ops->malloc_device) {
        int ret = current_vendor_ops->malloc_device(ctx, size, ptr);
        if (ret == 0) {
            LOG(ctx, GPUIO_LOG_DEBUG, "Allocated %zu bytes device memory at %p",
                size, *ptr);
            return GPUIO_SUCCESS;
        }
    }
    
    /* Fallback - allocate pinned and map */
    return gpuio_malloc_pinned(ctx, size, ptr);
}

gpuio_error_t gpuio_malloc_unified(gpuio_context_t ctx, size_t size, void** ptr) {
    if (!ctx || !ptr) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    /* Unified memory requires vendor support */
    if (current_vendor_ops && current_vendor_ops->malloc_device) {
        /* Use device malloc as fallback - not true unified memory */
        return gpuio_malloc_device(ctx, size, ptr);
    }
    
    return GPUIO_ERROR_UNSUPPORTED;
}

gpuio_error_t gpuio_free(gpuio_context_t ctx, void* ptr) {
    if (!ctx) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (!ptr) {
        return GPUIO_SUCCESS; /* Free of NULL is OK */
    }
    
    /* Check if this is a registered region */
    pthread_mutex_lock(&ctx->regions_lock);
    memory_region_internal_t* region = ctx->regions;
    while (region) {
        if (region->base_addr == ptr) {
            pthread_mutex_unlock(&ctx->regions_lock);
            /* Region should be unregistered first */
            return GPUIO_ERROR_BUSY;
        }
        region = region->next;
    }
    pthread_mutex_unlock(&ctx->regions_lock);
    
    /* Try vendor-specific free first */
    if (current_vendor_ops && current_vendor_ops->free) {
        int ret = current_vendor_ops->free(ctx, ptr);
        if (ret == 0) {
            LOG(ctx, GPUIO_LOG_DEBUG, "Freed memory at %p", ptr);
            return GPUIO_SUCCESS;
        }
    }
    
    /* Fallback to standard free */
    free(ptr);
    LOG(ctx, GPUIO_LOG_DEBUG, "Freed memory at %p", ptr);
    
    return GPUIO_SUCCESS;
}

/* ============================================================================
 * Memory Registration
 * ============================================================================ */

gpuio_error_t gpuio_register_memory(gpuio_context_t ctx, void* ptr, size_t size,
                                     gpuio_mem_access_t access,
                                     gpuio_memory_region_t* region) {
    if (!ctx || !region) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (!ptr || size == 0) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    /* Allocate internal region structure */
    memory_region_internal_t* internal = calloc(1, sizeof(memory_region_internal_t));
    if (!internal) {
        return GPUIO_ERROR_NOMEM;
    }
    
    internal->base_addr = ptr;
    internal->length = size;
    internal->access = access;
    internal->gpu_id = ctx->current_device;
    internal->registered = true;
    
    /* Try vendor-specific registration */
    if (current_vendor_ops && current_vendor_ops->register_memory) {
        int ret = current_vendor_ops->register_memory(ctx, ptr, size, access, internal);
        if (ret != 0) {
            free(internal);
            return GPUIO_ERROR_GENERAL;
        }
    } else {
        /* Software fallback - just track the region */
        internal->gpu_addr = ptr;
        internal->bus_addr = (uint64_t)(uintptr_t)ptr;
    }
    
    /* Add to context list */
    pthread_mutex_lock(&ctx->regions_lock);
    internal->next = ctx->regions;
    ctx->regions = internal;
    pthread_mutex_unlock(&ctx->regions_lock);
    
    /* Fill public structure */
    region->base_addr = internal->base_addr;
    region->gpu_addr = internal->gpu_addr;
    region->bus_addr = internal->bus_addr;
    region->length = internal->length;
    region->access = internal->access;
    region->gpu_id = internal->gpu_id;
    region->registered = true;
    region->handle = internal;
    
    LOG(ctx, GPUIO_LOG_DEBUG, "Registered memory region %p, size %zu",
        ptr, size);
    
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_unregister_memory(gpuio_context_t ctx, 
                                        gpuio_memory_region_t* region) {
    if (!ctx || !region) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    memory_region_internal_t* internal = (memory_region_internal_t*)region->handle;
    if (!internal) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    /* Remove from context list */
    pthread_mutex_lock(&ctx->regions_lock);
    memory_region_internal_t** current = &ctx->regions;
    while (*current) {
        if (*current == internal) {
            *current = internal->next;
            break;
        }
        current = &(*current)->next;
    }
    pthread_mutex_unlock(&ctx->regions_lock);
    
    /* Vendor-specific unregistration */
    if (current_vendor_ops && current_vendor_ops->unregister_memory) {
        current_vendor_ops->unregister_memory(ctx, internal);
    }
    
    internal->registered = false;
    region->registered = false;
    
    free(internal);
    
    LOG(ctx, GPUIO_LOG_DEBUG, "Unregistered memory region");
    
    return GPUIO_SUCCESS;
}

/* ============================================================================
 * Memory Copy
 * ============================================================================ */

gpuio_error_t gpuio_memcpy(gpuio_context_t ctx, void* dst, const void* src, 
                            size_t size, gpuio_stream_t stream) {
    if (!ctx) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (!dst || !src) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    /* Try vendor-specific memcpy first */
    if (current_vendor_ops && current_vendor_ops->memcpy) {
        int ret = current_vendor_ops->memcpy(ctx, dst, src, size, stream);
        if (ret == 0) {
            /* Update statistics */
            pthread_mutex_lock(&ctx->stats_lock);
            ctx->stats.bytes_written += size;
            pthread_mutex_unlock(&ctx->stats_lock);
            return GPUIO_SUCCESS;
        }
    }
    
    /* Fallback to standard memcpy */
    memcpy(dst, src, size);
    
    /* Update statistics */
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.bytes_written += size;
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_memcpy_async(gpuio_context_t ctx, void* dst, const void* src,
                                  size_t size, gpuio_stream_t stream) {
    /* For now, just call synchronous version */
    /* In full implementation, this would queue to stream */
    return gpuio_memcpy(ctx, dst, src, size, stream);
}
