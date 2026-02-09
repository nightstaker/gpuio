/**
 * @file internal.h
 * @brief Internal definitions for gpuio implementation
 * @version 1.0.0
 */

#ifndef GPUIO_INTERNAL_H
#define GPUIO_INTERNAL_H

#include "gpuio.h"
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/* GPU vendor detection */
typedef enum {
    GPU_VENDOR_UNKNOWN = 0,
    GPU_VENDOR_NVIDIA,
    GPU_VENDOR_AMD,
    GPU_VENDOR_INTEL,
} gpu_vendor_t;

/* Device information (internal) */
typedef struct {
    int device_id;
    gpu_vendor_t vendor;
    char name[256];
    size_t total_memory;
    size_t free_memory;
    int compute_capability_major;
    int compute_capability_minor;
    bool supports_gds;
    bool supports_gdr;
    bool supports_cxl;
    int numa_node;
    void* vendor_handle;  /* CUDA/HIP/SYCL context */
} device_info_internal_t;

/* Memory region (internal) */
typedef struct memory_region_internal {
    void* base_addr;
    void* gpu_addr;
    uint64_t bus_addr;
    size_t length;
    gpuio_mem_type_t type;
    gpuio_mem_access_t access;
    int gpu_id;
    bool registered;
    bool is_pinned;
    struct memory_region_internal* next;
} memory_region_internal_t;

/* Request (internal) */
typedef struct request_internal {
    uint64_t id;
    gpuio_request_type_t type;
    gpuio_io_engine_t engine;
    
    memory_region_internal_t* src;
    memory_region_internal_t* dst;
    uint64_t src_offset;
    uint64_t dst_offset;
    size_t length;
    
    gpuio_stream_t stream;
    gpuio_request_status_t status;
    gpuio_error_t error_code;
    size_t bytes_completed;
    
    gpuio_callback_t callback;
    void* user_data;
    
    /* Linked list for queueing */
    struct request_internal* next;
    struct request_internal* prev;
} request_internal_t;

/* Stream (internal) */
typedef struct {
    int id;
    gpuio_stream_priority_t priority;
    void* vendor_stream;  /* CUDA stream / HIP stream / SYCL queue */
    pthread_mutex_t lock;
    request_internal_t* pending_requests;
} stream_internal_t;

/* Context (internal implementation) */
struct gpuio_context {
    /* Configuration */
    gpuio_config_t config;
    int initialized;
    
    /* Devices */
    device_info_internal_t* devices;
    int num_devices;
    int current_device;
    
    /* Memory regions */
    memory_region_internal_t* regions;
    pthread_mutex_t regions_lock;
    
    /* Streams */
    stream_internal_t** streams;
    int num_streams;
    pthread_mutex_t streams_lock;
    
    /* Request management */
    uint64_t next_request_id;
    request_internal_t* active_requests;
    pthread_mutex_t requests_lock;
    
    /* Statistics */
    gpuio_stats_t stats;
    pthread_mutex_t stats_lock;
    
    /* Thread pool for async operations */
    void* thread_pool;
    
    /* Logging */
    gpuio_log_level_t log_level;
    FILE* log_file;
};

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/* Device detection and initialization */
int device_detect_all(gpuio_context_t ctx);
int device_init_nvidia(gpuio_context_t ctx, int device_id);
int device_init_amd(gpuio_context_t ctx, int device_id);
void device_cleanup(gpuio_context_t ctx);

/* Memory management */
int memory_region_register_internal(gpuio_context_t ctx, 
                                     memory_region_internal_t* region);
int memory_region_unregister_internal(gpuio_context_t ctx,
                                       memory_region_internal_t* region);

/* Request management */
request_internal_t* request_create_internal(gpuio_context_t ctx,
                                            const gpuio_request_params_t* params);
void request_destroy_internal(gpuio_context_t ctx, request_internal_t* req);
int request_submit_internal(gpuio_context_t ctx, request_internal_t* req);
int request_wait_internal(gpuio_context_t ctx, request_internal_t* req,
                          uint64_t timeout_us);

/* Statistics */
void stats_update(gpuio_context_t ctx, gpuio_request_type_t type,
                  size_t bytes, gpuio_error_t status);

/* Logging */
void log_message(gpuio_context_t ctx, gpuio_log_level_t level,
                 const char* file, int line, const char* fmt, ...);

#define LOG(ctx, level, ...) \
    do { if (level <= (ctx)->log_level) log_message(ctx, level, __FILE__, __LINE__, __VA_ARGS__); } while(0)

/* Vendor-specific implementations */
typedef struct {
    /* Device management */
    int (*device_init)(gpuio_context_t ctx, int device_id);
    int (*device_get_info)(gpuio_context_t ctx, int device_id, 
                           device_info_internal_t* info);
    int (*device_set_current)(gpuio_context_t ctx, int device_id);
    
    /* Memory management */
    int (*malloc_device)(gpuio_context_t ctx, size_t size, void** ptr);
    int (*malloc_pinned)(gpuio_context_t ctx, size_t size, void** ptr);
    int (*free)(gpuio_context_t ctx, void* ptr);
    int (*memcpy)(gpuio_context_t ctx, void* dst, const void* src, 
                  size_t size, gpuio_stream_t stream);
    int (*register_memory)(gpuio_context_t ctx, void* ptr, size_t size,
                           gpuio_mem_access_t access,
                           memory_region_internal_t* region);
    int (*unregister_memory)(gpuio_context_t ctx, memory_region_internal_t* region);
    
    /* Stream management */
    int (*stream_create)(gpuio_context_t ctx, stream_internal_t* stream,
                         gpuio_stream_priority_t priority);
    int (*stream_destroy)(gpuio_context_t ctx, stream_internal_t* stream);
    int (*stream_synchronize)(gpuio_context_t ctx, stream_internal_t* stream);
    int (*stream_query)(gpuio_context_t ctx, stream_internal_t* stream, bool* idle);
    
    /* Event management */
    int (*event_create)(gpuio_context_t ctx, gpuio_event_t* event);
    int (*event_destroy)(gpuio_context_t ctx, gpuio_event_t event);
    int (*event_record)(gpuio_context_t ctx, gpuio_event_t event,
                        stream_internal_t* stream);
    int (*event_synchronize)(gpuio_context_t ctx, gpuio_event_t event);
    int (*event_elapsed_time)(gpuio_context_t ctx, gpuio_event_t start,
                               gpuio_event_t end, float* ms);
} vendor_ops_t;

extern vendor_ops_t nvidia_ops;
extern vendor_ops_t amd_ops;
extern vendor_ops_t* current_vendor_ops;

#endif /* GPUIO_INTERNAL_H */
