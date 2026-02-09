# System Architecture Document (SAD)

## GPU-Initiated IO Accelerator (gpuio)

**Version:** 1.0  
**Date:** 2026-02-09  
**Status:** Draft

---

## 1. Architecture Overview

### 1.1 Design Philosophy

gpuio follows a layered, modular architecture with clear separation of concerns:

1. **User Interface Layer** - Provides both transparent (on-demand) and explicit API access
2. **Core Engine Layer** - Manages IO scheduling, caching, and resource allocation
3. **Transport Abstraction Layer** - Abstracts MemIO, LocalIO, and RemoteIO operations
4. **Hardware Abstraction Layer** - Interfaces with GPU drivers, RDMA NICs, and NVMe controllers
5. **Kernel Driver Layer** - Kernel-space components for privileged operations

### 1.2 High-Level Architecture

```
User Application Layer (Python/C/C++/CUDA)
                |
User Interface Layer (On-Demand Proxy / Explicit API / Batch Queue)
                |
Core Engine Layer (Metadata Manager / Cache Manager / Request Scheduler)
                |
Transport Layer (MemIO / LocalIO / RemoteIO)
                |
Hardware Abstraction (GPU Driver / NVMe Driver / RDMA Stack)
                |
Hardware Layer (GPU HBM / NVMe SSD / RDMA NIC / DRAM / CXL)
```

---

## 2. Component Architecture

### 2.1 On-Demand Proxy (Transparent IO)

The On-Demand Proxy provides lazy loading semantics for data access:

**Key Components:**
- **Operator Hook**: Intercepts array indexing operations using CUDA device functions
- **Memory Mapper**: Manages virtual address space mapping to physical GPU memory
- **Page Fault Handler**: Triggers data fetch on access to unmapped regions
- **Prefetcher**: Predicts and pre-loads data based on access patterns

### 2.2 Core Engine Components

#### 2.2.1 Request Scheduler

The scheduler implements multi-queue, priority-based dispatch:
- **Deadline-aware EDF** (Earliest Deadline First) for real-time requests
- **CFS-like fairness** for best-effort requests
- **Work conservation** - workers always have work if available

#### 2.2.2 Cache Manager

Multi-level caching architecture:
- **L1**: GPU Device Cache (HBM) - Direct-mapped, hardware coherency
- **L2**: Host Cache (DRAM) - Software-managed, compressed
- **L3**: Storage Cache (NVMe/SATA) - Block-based, write-back mode

---

## 3. Transport Layer Design

### 3.1 MemIO Transport

Zero-copy memory access with NUMA awareness:
- **Zero-Copy Path**: Direct GPU load/store to system memory
- **CXL Access**: Load/store to CXL memory expanders
- **Unified Memory**: CUDA/ROCm managed memory

### 3.2 LocalIO Transport

Direct GPU access to NVMe storage:
- **GPUDirect Storage (GDS)**: cuFile API for direct GPU-NVMe
- **SPDK Integration**: Userspace NVMe driver for kernel bypass
- **io_uring**: Linux async IO interface (fallback)

### 3.3 RemoteIO Transport

RDMA-based network IO with GPU-Direct:
- **IB Verbs**: Low-level InfiniBand/RoCE interface
- **libfabric**: Modern OpenFabrics interface
- **UCX**: Unified Communication X framework
- **GPUDirect RDMA**: Zero-copy GPU memory access over RDMA

---

## 4. Data Flow Architecture

### 4.1 On-Demand Access Flow

1. GPU kernel accesses data[i]
2. Page fault trap occurs (unmapped region)
3. Runtime looks up cache (miss)
4. Allocate GPU memory for cache line
5. Submit async IO request
6. Data transfer from source
7. Update page table
8. Resume GPU kernel

### 4.2 Explicit Transfer Flow

1. Application submits transfer request
2. Runtime validates and queues request
3. Memory registration (if needed)
4. Route to transport layer
5. Execute transfer
6. Completion notification
7. Invoke callback

---

## 5. Memory Architecture

### 5.1 Memory Hierarchy

From fastest/smallest to slowest/largest:
- **Registers** (1ns latency)
- **SM L1 Cache**
- **GPU L2 Cache**
- **GPU HBM (3+ TB/s)**
- **Host DRAM via PCIe/CXL (200 GB/s)**
- **CXL Memory Pool**
- **Local NVMe Array (50+ GB/s)**
- **RDMA Network (100 GB/s)**
- **Cloud Storage (PB+ scale)**

### 5.2 Address Space Mapping

GPU Virtual Address Space regions:
- GPU HBM: 0x0000_0000_0000_0000
- gpuio Window: 0x0000_7FFF_FFFF_FFFF
- Peer GPU HBM: 0x0001_0000_0000_0000
- System Memory: 0x0002_0000_0000_0000
- CXL Memory: 0x0003_0000_0000_0000
- NVMe BAR: 0x0004_0000_0000_0000
- NIC BAR: 0x0005_0000_0000_0000

---

## 6. Deployment Architecture

### 6.1 Single Node

Components per node:
- 1-16 GPUs (H100/H200 or equivalent)
- 2-8 NVMe SSDs (Gen4/Gen5)
- 1-2 RDMA NICs (ConnectX-7, 400Gbps)
- Optional: CXL Memory Expander
- PCIe Switch or CXL fabric

### 6.2 Multi-Node Cluster

- InfiniBand/RoCE network fabric
- Multiple compute nodes with GPUs
- Metadata service (etcd/ZooKeeper) optional
- Distributed caching across nodes

---

## 7. Security Architecture

| Layer | Components |
|-------|------------|
| Application | API Key Authentication, RBAC |
| Transport | TLS/SSL for Remote IO, RDMA PSK |
| Network | Firewall Rules, VLAN Isolation |
| Memory | Process Isolation, Capability-based Access |
| Hardware | SR-IOV, Trusted Execution, Secure Boot |

---

## 8. Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-02-09 | gpuio Team | Initial architecture |
