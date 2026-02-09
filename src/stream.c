/**
 * @file stream.c
 * @brief Stream and event management implementation
 * @version 1.0.0
 */

#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Stream Management
 * ============================================================================ */

gpuio_error_t gpuio_stream_create(gpuio_context_t ctx, gpuio_stream_t* stream,
                                   gpuio_stream_priority_t priority) {
    if (!ctx || !stream) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    /* Allocate internal stream structure */
    stream_internal_t* internal = calloc(1, sizeof(stream_internal_t));
    if (!internal) {
        return GPUIO_ERROR_NOMEM;
    }
    
    internal->priority = priority;
    pthread_mutex_init(&internal->lock, NULL);
    
    /* Try vendor-specific creation */
    if (current_vendor_ops && current_vendor_ops->stream_create) {
        int ret = current_vendor_ops->stream_create(ctx, internal, priority);
        if (ret != 0) {
            pthread_mutex_destroy(&internal->lock);
            free(internal);
            return GPUIO_ERROR_GENERAL;
        }
    }
    
    /* Add to context */
    pthread_mutex_lock(&ctx->streams_lock);
    
    /* Expand stream array if needed */
    int stream_id = ctx->num_streams;
    stream_internal_t** new_streams = realloc(ctx->streams, 
                                               (stream_id + 1) * sizeof(void*));
    if (!new_streams) {
        pthread_mutex_unlock(&ctx->streams_lock);
        if (current_vendor_ops && current_vendor_ops->stream_destroy) {
            current_vendor_ops->stream_destroy(ctx, internal);
        }
        pthread_mutex_destroy(&internal->lock);
        free(internal);
        return GPUIO_ERROR_NOMEM;
    }
    
    ctx->streams = new_streams;
    ctx->streams[stream_id] = internal;
    internal->id = stream_id;
    ctx->num_streams++;
    
    pthread_mutex_unlock(&ctx->streams_lock);
    
    *stream = (gpuio_stream_t)internal;
    
    LOG(ctx, GPUIO_LOG_DEBUG, "Created stream %d with priority %d",
        stream_id, priority);
    
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_stream_destroy(gpuio_context_t ctx, gpuio_stream_t stream) {
    if (!ctx || !stream) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    stream_internal_t* internal = (stream_internal_t*)stream;
    
    /* Vendor-specific destruction */
    if (current_vendor_ops && current_vendor_ops->stream_destroy) {
        current_vendor_ops->stream_destroy(ctx, internal);
    }
    
    pthread_mutex_destroy(&internal->lock);
    
    /* Note: We don't remove from array to keep IDs stable */
    /* Just mark as destroyed */
    internal->id = -1;
    free(internal);
    
    return GPUPIO_SUCCESS;
}

gpuio_error_t gpuio_stream_synchronize(gpuio_context_t ctx, gpuio_stream_t stream) {
    if (!ctx) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (!stream) {
        /* Synchronize all streams */
        pthread_mutex_lock(&ctx->streams_lock);
        for (int i = 0; i < ctx->num_streams; i++) {
            if (ctx->streams[i] && ctx->streams[i]->id >= 0) {
                if (current_vendor_ops && current_vendor_ops->stream_synchronize) {
                    current_vendor_ops->stream_synchronize(ctx, ctx->streams[i]);
                }
            }
        }
        pthread_mutex_unlock(&ctx->streams_lock);
        return GPUIO_SUCCESS;
    }
    
    stream_internal_t* internal = (stream_internal_t*)stream;
    
    if (current_vendor_ops && current_vendor_ops->stream_synchronize) {
        int ret = current_vendor_ops->stream_synchronize(ctx, internal);
        if (ret != 0) {
            return GPUIO_ERROR_GENERAL;
        }
    }
    
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_stream_query(gpuio_context_t ctx, gpuio_stream_t stream, 
                                  bool* idle) {
    if (!ctx || !stream || !idle) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    stream_internal_t* internal = (stream_internal_t*)stream;
    
    if (current_vendor_ops && current_vendor_ops->stream_query) {
        int ret = current_vendor_ops->stream_query(ctx, internal, idle);
        if (ret != 0) {
            return GPUIO_ERROR_GENERAL;
        }
    } else {
        /* Software fallback - assume idle */
        *idle = true;
    }
    
    return GPUIO_SUCCESS;
}

/* ============================================================================
 * Event Management
 * ============================================================================ */

struct gpuio_event {
    void* vendor_event;
    uint64_t timestamp;
};

gpuio_error_t gpuio_event_create(gpuio_context_t ctx, gpuio_event_t* event) {
    if (!ctx || !event) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    gpuio_event_t ev = calloc(1, sizeof(struct gpuio_event));
    if (!ev) {
        return GPUIO_ERROR_NOMEM;
    }
    
    if (current_vendor_ops && current_vendor_ops->event_create) {
        int ret = current_vendor_ops->event_create(ctx, &ev);
        if (ret != 0) {
            free(ev);
            return GPUIO_ERROR_GENERAL;
        }
    }
    
    *event = ev;
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_event_destroy(gpuio_context_t ctx, gpuio_event_t event) {
    if (!ctx || !event) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (current_vendor_ops && current_vendor_ops->event_destroy) {
        current_vendor_ops->event_destroy(ctx, event);
    }
    
    free(event);
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_event_record(gpuio_context_t ctx, gpuio_event_t event, 
                                  gpuio_stream_t stream) {
    if (!ctx || !event || !stream) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    stream_internal_t* internal = (stream_internal_t*)stream;
    
    if (current_vendor_ops && current_vendor_ops->event_record) {
        int ret = current_vendor_ops->event_record(ctx, event, internal);
        if (ret != 0) {
            return GPUIO_ERROR_GENERAL;
        }
    }
    
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_event_synchronize(gpuio_context_t ctx, gpuio_event_t event) {
    if (!ctx || !event) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (current_vendor_ops && current_vendor_ops->event_synchronize) {
        int ret = current_vendor_ops->event_synchronize(ctx, event);
        if (ret != 0) {
            return GPUIO_ERROR_GENERAL;
        }
    }
    
    return GPUIO_SUCCESS;
}

gpuio_error_t gpuio_event_elapsed_time(gpuio_context_t ctx, gpuio_event_t start,
                                        gpuio_event_t end, float* ms) {
    if (!ctx || !start || !end || !ms) {
        return GPUIO_ERROR_INVALID_ARG;
    }
    
    if (!ctx->initialized) {
        return GPUIO_ERROR_NOT_INITIALIZED;
    }
    
    if (current_vendor_ops && current_vendor_ops->event_elapsed_time) {
        int ret = current_vendor_ops->event_elapsed_time(ctx, start, end, ms);
        if (ret != 0) {
            return GPUIO_ERROR_GENERAL;
        }
    } else {
        /* Software fallback - use timestamps if available */
        *ms = 0.0f;
    }
    
    return GPUIO_SUCCESS;
}
