"""
gpuio - GPU-Initiated IO Accelerator for AI/ML

This package provides Python bindings for gpuio, a high-performance 
GPU-initiated IO acceleration framework designed for:
- Large-scale distributed AI training
- Real-time and batch inference serving
- DeepSeek DSA KV Cache Management
- Graph RAG for Knowledge-Augmented LLMs
- DeepSeek Engram Memory Architecture

Example:
    >>> import gpuio
    >>> 
    >>> # Initialize context
    >>> ctx = gpuio.Context({"log_level": gpuio.LOG_INFO})
    >>> 
    >>> # Get device count
    >>> print(f"GPUs: {ctx.get_device_count()}")
    >>> 
    >>> # Allocate memory
    >>> mem = ctx.malloc(1024 * 1024)  # 1MB
    >>> pinned = ctx.malloc_pinned(1024 * 1024)
    >>> 
    >>> # Copy memory
    >>> ctx.memcpy(mem, pinned, 1024 * 1024)
    >>> ctx.synchronize()
    >>> 
    >>> # Cleanup
    >>> ctx.free(mem)
    >>> ctx.free(pinned)
    >>> 
    >>> # AI Context for DeepSeek features
    >>> ai_ctx = gpuio.AIContext(ctx, {
    ...     "num_layers": 24,
    ...     "num_heads": 16,
    ...     "enable_dsa_kv": True,
    ...     "enable_engram": True
    ... })
"""

from .gpuio import (
    Context,
    AIContext,
    GPUIOError,
    LOG_NONE,
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    PRIO_INFERENCE_REALTIME,
    PRIO_INFERENCE_BATCH,
    PRIO_TRAINING_FW,
    PRIO_TRAINING_BW,
)

__version__ = "1.1.0"
__all__ = [
    "Context",
    "AIContext",
    "GPUIOError",
    "LOG_NONE",
    "LOG_FATAL",
    "LOG_ERROR",
    "LOG_WARN",
    "LOG_INFO",
    "LOG_DEBUG",
    "PRIO_INFERENCE_REALTIME",
    "PRIO_INFERENCE_BATCH",
    "PRIO_TRAINING_FW",
    "PRIO_TRAINING_BW",
]
