# System Design Document (SDD)

## GPU-Initiated IO Accelerator (gpuio)

**Version:** 1.0  
**Date:** 2026-02-09  
**Status:** Draft

---

## 1. Design Principles

### 1.1 Core Design Tenets

1. **Zero-Copy First** - Minimize data copies between GPU, CPU, and storage
2. **GPU-Centric Control** - GPU kernels drive IO operations, not the host
3. **Unified Abstraction** - Single API for memory, storage, and network IO
4. **Performance Predictability** - Consistent latency and throughput guarantees
5. **Fault Isolation** - Failures in one path don't affect others

---

## 2. Module Design

### 2.1 Module Hierarchy

```
gpuio/
├── core/                    # Core framework
│   ├── include/gpuio.h     # Main public API
│   └── src/
│       ├── context.c       # Context management
│       ├── memory_pool.c   # GPU memory pool
│       ├── scheduler.c     # Request scheduler
│       └── cache_manager.c # Cache management
├── transport/              # Transport implementations
│   ├── memio/             # Memory transport (CXL/UM)
│   ├── localio/           # Local storage (GDS/SPDK)
│   └── remoteio/          # Network transport (RDMA)
├── ondemand/              # On-demand (transparent) IO
├── api/                   # Language bindings
│   ├── python/
│   └── cpp/
├── kernel/                # Kernel modules (optional)
├── tools/                 # CLI tools
└── tests/                 # Test suite
```

---

## 3. Core Module Design

### 3.1 Context Management

```c
typedef struct gpuio_context {
    uint32_t magic;
    uint32_t version;
    int num_gpus;
    gpuio_gpu_t* gpus;
    gpuio_config_t config;
    struct scheduler* scheduler;
    struct cache_manager* cache;
    struct memory_pool* mem_pool;
    gpuio_stats_t stats;
    pthread_mutex_t lock;
    atomic_bool initialized;
} gpuio_context_t;

int gpuio_init(gpuio_context_t** ctx, const gpuio_config_t* config);
int gpuio_finalize(gpuio_context_t* ctx);
```

### 3.2 Request Scheduler

```c
typedef struct io_request {
    uint64_t id;
    gpuio_op_t op;
    gpuio_transport_t transport;
    gpuio_buffer_t src;
    gpuio_buffer_t dst;
    size_t size;
    int priority;
    uint64_t deadline_ns;
    atomic_int state;
    gpuio_callback_t on_complete;
    void* user_data;
} io_request_t;
```

---

## 4. Transport Layer Design

### 4.1 Transport Abstraction

```c
typedef struct transport_ops {
    const char* name;
    int (*init)(struct transport* t, gpuio_context_t* ctx);
    int (*read)(struct transport* t, gpuio_handle_t handle,
                void* buf, size_t offset, size_t size,
                gpuio_callback_t cb, void* user_data);
    int (*write)(struct transport* t, gpuio_handle_t handle,
                 const void* buf, size_t offset, size_t size,
                 gpuio_callback_t cb, void* user_data);
} transport_ops_t;
```

### 4.2 MemIO Transport

Zero-copy memory access:
- Zero-copy path: Direct GPU load/store
- CXL support: CXL.mem operations
- Unified Memory: CUDA/ROCm UM

### 4.3 LocalIO Transport

Direct GPU-NVMe:
- GPUDirect Storage (GDS)
- SPDK (userspace NVMe)
- io_uring fallback

### 4.4 RemoteIO Transport

RDMA networking:
- IB Verbs
- libfabric
- UCX
- GPUDirect RDMA

---

## 5. On-Demand IO Design

### 5.1 Proxy Object

```c
typedef struct gpuio_proxy {
    char uri[GPUIO_MAX_URI_LEN];
    size_t element_size;
    size_t num_elements;
    gpuio_transport_t transport;
    void* base_gpu_ptr;
    atomic_int state;
} gpuio_proxy_t;
```

### 5.2 Page Fault Handler

- GPU kernel triggers page fault
- Host worker processes fault queue
- Async IO fetches data
- GPU resumes after arrival

### 5.3 Prefetcher

- Tracks access patterns
- Predicts future accesses
- Initiates low-priority prefetches

---

## 6. API Design

### 6.1 C API

```c
// Context
int gpuio_init(gpuio_context_t* ctx, const gpuio_config_t* config);
int gpuio_finalize(gpuio_context_t ctx);

// On-Demand
int gpuio_proxy_create(gpuio_context_t ctx, const char* uri,
                       gpuio_proxy_t* proxy);
int gpuio_proxy_prefetch(gpuio_proxy_t proxy, size_t start, size_t count);
int gpuio_proxy_sync(gpuio_proxy_t proxy);

// Explicit Transfer
int gpuio_open(gpuio_context_t ctx, const char* uri, gpuio_handle_t* handle);
int gpuio_read(gpuio_context_t ctx, gpuio_handle_t src, size_t offset,
               void* dst, size_t size);
int gpuio_write(gpuio_context_t ctx, gpuio_handle_t dst, size_t offset,
                const void* src, size_t size);
int gpuio_wait(gpuio_request_t request);
```

### 6.2 Python API

```python
import gpuio

# On-demand
data = gpuio.open("rdma://server/dataset", shape=(N,), dtype=np.float32)
result = kernel(data[i:j])

# Explicit
with gpuio.Context() as ctx:
    ctx.read("nvme:///path/file", buffer)
```

---

## 7. URI Scheme

```
mem://localhost/region?pin=true
nvme:///dev/nvme0n1/file
rdma://host:port/path?use_gdr=true
file:///local/path
```

---

## 8. Build System

```cmake
cmake_minimum_required(VERSION 3.18)
project(gpuio VERSION 1.0.0 LANGUAGES C CXX CUDA)

option(GPUIO_BUILD_TESTS "Build tests" ON)
option(GPUIO_ENABLE_GDS "Enable GPUDirect Storage" ON)
option(GPUIO_ENABLE_CXL "Enable CXL support" OFF)

find_package(CUDAToolkit 11.0 REQUIRED)

add_subdirectory(core)
add_subdirectory(transport)
add_subdirectory(ondemand)
```

---

## 9. Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-09 | gpuio Team | Initial design |
