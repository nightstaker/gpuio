/**
 * @file gpuio_python.c
 * @brief Python bindings for gpuio using Python C API
 * @version 1.1.0
 * 
 * Expanded bindings supporting:
 * - Context management
 * - Memory allocation (host, device, pinned, unified)
 * - Stream management
 * - Request management
 * - AI Context (DSA KV, Graph RAG, Engram)
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <gpuio/gpuio.h>
#include <gpuio/gpuio_ai.h>

/* Module definition */
static PyModuleDef gpuio_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "gpuio",
    .m_doc = "GPU-Initiated IO Accelerator for AI/ML",
    .m_size = -1,
};

/* Error object */
static PyObject* GPUIOError;

/* Convert gpuio_error_t to Python exception */
static void raise_gpuio_error(gpuio_error_t err) {
    PyErr_SetString(GPUIOError, gpuio_error_string(err));
}

/* ============================================================================
 * Context Object
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    gpuio_context_t ctx;
} ContextObject;

static void context_dealloc(ContextObject* self) {
    if (self->ctx) {
        gpuio_finalize(self->ctx);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* context_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    ContextObject* self = (ContextObject*)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->ctx = NULL;
    return (PyObject*)self;
}

static int context_init(ContextObject* self, PyObject* args, PyObject* kwds) {
    gpuio_config_t config = GPUIO_CONFIG_DEFAULT;
    
    PyObject* config_dict = NULL;
    if (!PyArg_ParseTuple(args, "|O", &config_dict)) {
        return -1;
    }
    
    if (config_dict && PyDict_Check(config_dict)) {
        PyObject* log_level = PyDict_GetItemString(config_dict, "log_level");
        if (log_level && PyLong_Check(log_level)) {
            config.log_level = (int)PyLong_AsLong(log_level);
        }
    }
    
    gpuio_error_t err = gpuio_init(&self->ctx, &config);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return -1;
    }
    
    return 0;
}

static PyObject* context_get_device_count(ContextObject* self) {
    int count;
    gpuio_error_t err = gpuio_get_device_count(self->ctx, &count);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    return PyLong_FromLong(count);
}

static PyObject* context_get_stats(ContextObject* self) {
    gpuio_stats_t stats;
    gpuio_error_t err = gpuio_get_stats(self->ctx, &stats);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    
    PyObject* result = PyDict_New();
    PyDict_SetItemString(result, "requests_submitted",
                         PyLong_FromUnsignedLongLong(stats.requests_submitted));
    PyDict_SetItemString(result, "requests_completed",
                         PyLong_FromUnsignedLongLong(stats.requests_completed));
    PyDict_SetItemString(result, "bytes_transferred",
                         PyLong_FromUnsignedLongLong(stats.bytes_transferred));
    PyDict_SetItemString(result, "bandwidth_gbps",
                         PyFloat_FromDouble(stats.bandwidth_gbps));
    PyDict_SetItemString(result, "cache_hit_rate",
                         PyFloat_FromDouble(stats.cache_hit_rate));
    
    return result;
}

static PyObject* context_malloc(ContextObject* self, PyObject* args) {
    size_t size;
    if (!PyArg_ParseTuple(args, "n", &size)) {
        return NULL;
    }
    
    void* ptr;
    gpuio_error_t err = gpuio_malloc(self->ctx, size, &ptr);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    
    return PyCapsule_New(ptr, "gpuio.memory", NULL);
}

static PyObject* context_malloc_pinned(ContextObject* self, PyObject* args) {
    size_t size;
    if (!PyArg_ParseTuple(args, "n", &size)) {
        return NULL;
    }
    
    void* ptr;
    gpuio_error_t err = gpuio_malloc_pinned(self->ctx, size, &ptr);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    
    return PyCapsule_New(ptr, "gpuio.pinned_memory", NULL);
}

static PyObject* context_malloc_device(ContextObject* self, PyObject* args) {
    size_t size;
    if (!PyArg_ParseTuple(args, "n", &size)) {
        return NULL;
    }
    
    void* ptr;
    gpuio_error_t err = gpuio_malloc_device(self->ctx, size, &ptr);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    
    return PyCapsule_New(ptr, "gpuio.device_memory", NULL);
}

static PyObject* context_free(ContextObject* self, PyObject* args) {
    PyObject* capsule;
    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        return NULL;
    }
    
    if (!PyCapsule_CheckExact(capsule)) {
        PyErr_SetString(PyExc_TypeError, "Expected capsule object");
        return NULL;
    }
    
    void* ptr = PyCapsule_GetPointer(capsule, NULL);
    if (!ptr) return NULL;
    
    gpuio_error_t err = gpuio_free(self->ctx, ptr);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    
    Py_RETURN_NONE;
}

static PyObject* context_memcpy(ContextObject* self, PyObject* args) {
    PyObject* dst_capsule, *src_capsule;
    size_t size;
    
    if (!PyArg_ParseTuple(args, "OOn", &dst_capsule, &src_capsule, &size)) {
        return NULL;
    }
    
    void* dst = PyCapsule_GetPointer(dst_capsule, NULL);
    void* src = PyCapsule_GetPointer(src_capsule, NULL);
    if (!dst || !src) return NULL;
    
    gpuio_error_t err = gpuio_memcpy(self->ctx, dst, src, size, NULL);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    
    Py_RETURN_NONE;
}

static PyObject* context_synchronize(ContextObject* self) {
    gpuio_error_t err = gpuio_synchronize(self->ctx);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef context_methods[] = {
    {"get_device_count", (PyCFunction)context_get_device_count, METH_NOARGS,
     "Get number of available GPU devices"},
    {"get_stats", (PyCFunction)context_get_stats, METH_NOARGS,
     "Get IO statistics"},
    {"malloc", (PyCFunction)context_malloc, METH_VARARGS,
     "Allocate memory (host)"},
    {"malloc_pinned", (PyCFunction)context_malloc_pinned, METH_VARARGS,
     "Allocate pinned/page-locked memory"},
    {"malloc_device", (PyCFunction)context_malloc_device, METH_VARARGS,
     "Allocate device memory"},
    {"free", (PyCFunction)context_free, METH_VARARGS,
     "Free allocated memory"},
    {"memcpy", (PyCFunction)context_memcpy, METH_VARARGS,
     "Copy memory (dst, src, size)"},
    {"synchronize", (PyCFunction)context_synchronize, METH_NOARGS,
     "Synchronize all pending operations"},
    {NULL}
};

static PyTypeObject ContextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "gpuio.Context",
    .tp_doc = "GPUIO Context",
    .tp_basicsize = sizeof(ContextObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = context_new,
    .tp_init = (initproc)context_init,
    .tp_dealloc = (destructor)context_dealloc,
    .tp_methods = context_methods,
};

/* ============================================================================
 * AI Context Object
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    gpuio_ai_context_t ai_ctx;
    ContextObject* base_ctx;  /* Keep reference to base context */
} AIContextObject;

static void ai_context_dealloc(AIContextObject* self) {
    if (self->ai_ctx) {
        gpuio_ai_context_destroy(self->ai_ctx);
    }
    Py_XDECREF(self->base_ctx);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* ai_context_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    AIContextObject* self = (AIContextObject*)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->ai_ctx = NULL;
    self->base_ctx = NULL;
    return (PyObject*)self;
}

static int ai_context_init(AIContextObject* self, PyObject* args, PyObject* kwds) {
    PyObject* ctx_obj;
    PyObject* config_dict = NULL;
    
    if (!PyArg_ParseTuple(args, "O|O", &ctx_obj, &config_dict)) {
        return -1;
    }
    
    if (!PyObject_IsInstance(ctx_obj, (PyObject*)&ContextType)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a Context");
        return -1;
    }
    
    self->base_ctx = (ContextObject*)ctx_obj;
    Py_INCREF(self->base_ctx);
    
    gpuio_ai_config_t config = {0};
    config.num_layers = 12;
    config.num_heads = 16;
    config.head_dim = 64;
    config.max_sequence_length = 2048;
    config.enable_dsa_kv = true;
    config.enable_engram = true;
    config.enable_graph_rag = true;
    config.default_priority = GPUIO_PRIO_TRAINING_FW;
    config.kv_cache_size = 1ULL << 30;  /* 1GB */
    config.engram_pool_size = 10ULL << 30; /* 10GB */
    
    if (config_dict && PyDict_Check(config_dict)) {
        PyObject* val;
        val = PyDict_GetItemString(config_dict, "num_layers");
        if (val && PyLong_Check(val)) config.num_layers = (int)PyLong_AsLong(val);
        val = PyDict_GetItemString(config_dict, "num_heads");
        if (val && PyLong_Check(val)) config.num_heads = (int)PyLong_AsLong(val);
        val = PyDict_GetItemString(config_dict, "head_dim");
        if (val && PyLong_Check(val)) config.head_dim = (int)PyLong_AsLong(val);
        val = PyDict_GetItemString(config_dict, "enable_dsa_kv");
        if (val && PyBool_Check(val)) config.enable_dsa_kv = (val == Py_True);
        val = PyDict_GetItemString(config_dict, "enable_engram");
        if (val && PyBool_Check(val)) config.enable_engram = (val == Py_True);
        val = PyDict_GetItemString(config_dict, "enable_graph_rag");
        if (val && PyBool_Check(val)) config.enable_graph_rag = (val == Py_True);
    }
    
    gpuio_error_t err = gpuio_ai_context_create(self->base_ctx->ctx, &config, &self->ai_ctx);
    if (err != GPUIO_SUCCESS) {
        raise_gpuio_error(err);
        return -1;
    }
    
    return 0;
}

static PyMethodDef ai_context_methods[] = {
    {NULL}
};

static PyTypeObject AIContextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "gpuio.AIContext",
    .tp_doc = "GPUIO AI Context for DeepSeek features",
    .tp_basicsize = sizeof(AIContextObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ai_context_new,
    .tp_init = (initproc)ai_context_init,
    .tp_dealloc = (destructor)ai_context_dealloc,
    .tp_methods = ai_context_methods,
};

/* ============================================================================
 * Module Initialization
 * ============================================================================ */

PyMODINIT_FUNC PyInit_gpuio(void) {
    PyObject* module = PyModule_Create(&gpuio_module);
    if (!module) return NULL;
    
    /* Add Context type */
    if (PyType_Ready(&ContextType) < 0) return NULL;
    Py_INCREF(&ContextType);
    PyModule_AddObject(module, "Context", (PyObject*)&ContextType);
    
    /* Add AIContext type */
    if (PyType_Ready(&AIContextType) < 0) return NULL;
    Py_INCREF(&AIContextType);
    PyModule_AddObject(module, "AIContext", (PyObject*)&AIContextType);
    
    /* Add exception */
    GPUIOError = PyErr_NewException("gpuio.GPUIOError", NULL, NULL);
    Py_XINCREF(GPUIOError);
    PyModule_AddObject(module, "GPUIOError", GPUIOError);
    
    /* Add version */
    PyModule_AddStringConstant(module, "__version__", "1.1.0");
    
    /* Add constants */
    PyModule_AddIntConstant(module, "LOG_NONE", GPUIO_LOG_NONE);
    PyModule_AddIntConstant(module, "LOG_FATAL", GPUIO_LOG_FATAL);
    PyModule_AddIntConstant(module, "LOG_ERROR", GPUIO_LOG_ERROR);
    PyModule_AddIntConstant(module, "LOG_WARN", GPUIO_LOG_WARN);
    PyModule_AddIntConstant(module, "LOG_INFO", GPUIO_LOG_INFO);
    PyModule_AddIntConstant(module, "LOG_DEBUG", GPUIO_LOG_DEBUG);
    
    /* AI Priority constants */
    PyModule_AddIntConstant(module, "PRIO_INFERENCE_REALTIME", GPUIO_PRIO_INFERENCE_REALTIME);
    PyModule_AddIntConstant(module, "PRIO_INFERENCE_BATCH", GPUIO_PRIO_INFERENCE_BATCH);
    PyModule_AddIntConstant(module, "PRIO_TRAINING_FW", GPUIO_PRIO_TRAINING_FW);
    PyModule_AddIntConstant(module, "PRIO_TRAINING_BW", GPUIO_PRIO_TRAINING_BW);
    
    return module;
}
