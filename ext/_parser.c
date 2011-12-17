#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <http_parser.h>
#include <stdio.h>

static PyObject * PyExc_HTTPParseError;

static int on_message_begin(http_parser* parser)
{
    int fail = 0;
    PyObject* self = (PyObject*)parser->data;
    if (PyObject_HasAttrString(self, "_on_message_begin")) {
        PyObject* callable = PyObject_GetAttrString(self, "_on_message_begin");
        PyObject* result = PyObject_CallObject(callable, NULL);
        if (PyObject_IsTrue(result))
            fail = 1;
        Py_XDECREF(result);
        Py_DECREF(callable);
    }
    return fail;
}

static int on_message_complete(http_parser* parser)
{
    int fail = 0;
    PyObject* self = (PyObject*)parser->data;
    if (PyObject_HasAttrString(self, "_on_message_complete")) {
        PyObject* callable = PyObject_GetAttrString(self, "_on_message_complete");
        PyObject* result = PyObject_CallObject(callable, NULL);
        if (PyObject_IsTrue(result))
            fail = 1;
        Py_XDECREF(result);
        Py_DECREF(callable);
    }
    return fail;
}

static int on_headers_complete(http_parser* parser)
{
    int skip_body = 0;
    PyObject* self = (PyObject*)parser->data;
    if (PyObject_HasAttrString(self, "_on_headers_complete")) {
        PyObject* callable = PyObject_GetAttrString(self, "_on_headers_complete");
        PyObject* result = PyObject_CallObject(callable, NULL);
        if (PyObject_IsTrue(result))
            skip_body = 1;
        Py_XDECREF(result);
        Py_DECREF(callable);
    }
    return skip_body;
}

static int on_header_field(http_parser* parser, const char *at, size_t length)
{
    int fail = 0;
    PyObject* self = (PyObject*)parser->data;
    if (PyObject_HasAttrString(self, "_on_header_field")) {
        PyObject* callable = PyObject_GetAttrString(self, "_on_header_field");
        PyObject* args = Py_BuildValue("(s#)", at, length);
        PyObject* result = PyObject_CallObject(callable, args);
        if (PyObject_IsTrue(result))
            fail = 1;
        Py_XDECREF(result);
        Py_DECREF(callable);
        Py_DECREF(args);
    }
    return fail;
}

static int on_header_value(http_parser* parser, const char *at, size_t length)
{
    int fail = 0;
    PyObject* self = (PyObject*)parser->data;
    if (PyObject_HasAttrString(self, "_on_header_value")) {
        PyObject* callable = PyObject_GetAttrString(self, "_on_header_value");
        PyObject* args = Py_BuildValue("(s#)", at, length);
        PyObject* result = PyObject_CallObject(callable, args);
        if (PyObject_IsTrue(result))
            fail = 1;
        Py_XDECREF(result);
        Py_DECREF(callable);
        Py_DECREF(args);
    }
    return fail;
}

static int on_body(http_parser* parser, const char *at, size_t length)
{
    int fail = 0;
    PyObject* self = (PyObject*)parser->data;
    if (PyObject_HasAttrString(self, "_on_body")) {
        PyObject* callable = PyObject_GetAttrString(self, "_on_body");
        PyObject* bytearray = PyByteArray_FromStringAndSize(at, length);
        PyObject* result = PyObject_CallFunctionObjArgs(
            callable, bytearray, NULL);
        if (PyObject_IsTrue(result))
            fail = 1;
        Py_XDECREF(result);
        Py_DECREF(callable);
        Py_DECREF(bytearray);
    }
    return fail;
}

static http_parser_settings _parser_settings = {
    on_message_begin,
    NULL, // on_url
    on_header_field,
    on_header_value,
    on_headers_complete,
    on_body,
    on_message_complete
};

typedef struct {
    PyObject_HEAD
    http_parser* parser;
} PyHTTPResponseParser;

static PyObject*
PyHTTPResponseParser_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    PyHTTPResponseParser* self = (PyHTTPResponseParser*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->parser = PyMem_Malloc(sizeof(http_parser));
        if (self->parser == NULL) {
            return NULL;
        } else {
            self->parser->data = (void*)self;
            http_parser_init(self->parser, HTTP_RESPONSE);
        }
    }
    return (PyObject*) self;
}

static size_t size_t_MAX = -1;

static PyObject*
PyHTTPResponseParser_feed(PyHTTPResponseParser *self, PyObject* args)
{
    char* buf = NULL;
    Py_ssize_t buf_len;
    int succeed = PyArg_ParseTuple(args, "t#", &buf, &buf_len);
    /* cast Py_ssize_t signed integer to unsigned */
    size_t unsigned_buf_len = buf_len + size_t_MAX + 1;
    if (succeed) {
        size_t nread = http_parser_execute(self->parser,
                &_parser_settings, buf, unsigned_buf_len);
        if (self->parser->http_errno != HPE_OK) {
            PyObject* msg = PyString_FromString(
                    http_errno_description(self->parser->http_errno));
            if (msg == NULL) return PyErr_NoMemory();
            PyErr_SetObject(PyExc_HTTPParseError, msg);
            Py_DECREF(msg);
            return NULL;
        }
        return Py_BuildValue("l", nread);
    }
    return NULL;
}

static PyObject*
PyHTTPResponseParser_get_code(PyHTTPResponseParser *self)
{
    return Py_BuildValue("i", self->parser->status_code);
}

static PyObject*
PyHTTPResponseParser_should_keep_alive(PyHTTPResponseParser* self)
{
    return Py_BuildValue("i", http_should_keep_alive(self->parser));
}

void
PyHTTPResponseParser_dealloc(PyHTTPResponseParser* self)
{
    self->parser->data = NULL;
    PyMem_Free(self->parser);
    self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef PyHTTPResponseParser_methods[] = {
    {"feed", (PyCFunction)PyHTTPResponseParser_feed, METH_VARARGS,
        "Feed the parser with data"},
    {"get_code", (PyCFunction)PyHTTPResponseParser_get_code, METH_NOARGS,
        "Get http response code"},
    {"should_keep_alive", (PyCFunction)PyHTTPResponseParser_should_keep_alive,
        METH_NOARGS,
        "Tell wether the connection should stay connected (HTTP 1.1)"},
    {NULL}  /* Sentinel */
};

static PyTypeObject HTTPParserType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "HTTPResponseParser",      /*tp_name*/
    sizeof(PyHTTPResponseParser),      /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyHTTPResponseParser_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "HTTP Response Parser instance (non thread-safe)",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    PyHTTPResponseParser_methods,      /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyHTTPResponseParser_new,                 /* tp_new */
};

static PyMethodDef module_methods[] = {
    {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_parser(void) 
{
    PyObject* module;

    if (PyType_Ready(&HTTPParserType) < 0)
        return;

    module = Py_InitModule3("_parser", module_methods,
                       "HTTP Parser from nginx/Joyent.");

    Py_INCREF(&HTTPParserType);
    PyModule_AddObject(module, "HTTPResponseParser", (PyObject *)&HTTPParserType);

    PyExc_HTTPParseError = PyErr_NewException(
            "_parser.HTTPParseError", NULL, NULL);
    Py_INCREF(PyExc_HTTPParseError);
    PyModule_AddObject(module, "HTTPParseError", PyExc_HTTPParseError);
}

#undef PY_SSIZE_T_CLEAN