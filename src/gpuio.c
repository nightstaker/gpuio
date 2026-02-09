/**
 * @file gpuio.c
 * @brief GPU-Initiated IO Accelerator - Main entry point
 * @version 1.0.0
 */

/* Core implementation files are split into modules:
 * - context.c: Context and device management
 * - memory.c: Memory allocation and registration
 * - stream.c: Stream and event management
 * - log.c: Logging, errors, and statistics
 * - vendor_nvidia.c: NVIDIA CUDA support
 * - vendor_amd.c: AMD ROCm support
 * 
 * This file serves as the main entry point for the library.
 */

#include "internal.h"

/* Pull in all implementation modules */
/* These would normally be compiled separately and linked together */
/* For single-file compilation, they could be included here */

/* Version information is defined in log.c */
extern const char* gpuio_get_version_string(void);

/* Library initialization state */
static int library_initialized = 0;

/**
 * Library constructor - called automatically when library is loaded
 */
__attribute__((constructor))
static void gpuio_library_init(void) {
    if (!library_initialized) {
        /* Global initialization */
        library_initialized = 1;
    }
}

/**
 * Library destructor - called automatically when library is unloaded
 */
__attribute__((destructor))
static void gpuio_library_fini(void) {
    if (library_initialized) {
        /* Global cleanup */
        library_initialized = 0;
    }
}
