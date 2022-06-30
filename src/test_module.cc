#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <memory>
#include <string>
#include <vector>

#define MODULE_NAME test_module

#define STR1(x) #x
#define STR(x) STR1(x)

#define CAT1(a,b) a##b
#define CAT(a,b) CAT1(a,b)

namespace {

template <typename T>
struct py_class {
  PyObject_HEAD
private:
  union { T impl; }; // prevent default initialization of impl
  bool constructed;
public:
  template <typename... A>
  void construct(A&&... args) {
    constructed = false;
    std::construct_at(&impl,static_cast<A&&>(args)...);
    constructed = true;
  }
  void destroy() {
    if (constructed) {
      constructed = false;
      std::destroy_at(&impl);
    }
  }
  ~py_class() { destroy(); }
  operator bool() { return constructed; }
  T* operator->() { return &impl; }
  const T* operator->() const { return &impl; }
  T& operator*() { return impl; }
  const T& operator*() const { return impl; }
};

template <typename T>
void class_dealloc(py_class<T>* self) {
  self->destroy();
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

template <typename T>
int class_init(
  py_class<T>* self, PyTupleObject* args, PyObject* kwargs
);

template <>
int class_init<std::vector<std::string>>(
  py_class<std::vector<std::string>>* self,
  PyTupleObject* args,
  PyObject* kwargs
) {
  const int nargs = Py_SIZE(args);
  if (nargs<1) {
    PyErr_SetString(PyExc_ValueError,
      "string_vector must be given at least 1 argument");
    return -1;
  }

  try {
    self->construct(nargs);
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
    return -1;
  }
  auto& vec = **self;

  for (int i=0; i<nargs; ++i) {
    Py_ssize_t len;
    const char* str = PyUnicode_AsUTF8AndSize(args->ob_item[i],&len);
    if (!str) {
      self->destroy();
      PyErr_SetString(PyExc_ValueError,
        "all string_vector arguments must be strings");
      return -1;
    }
    vec[i] = { str, (size_t)len };
  }

  return 0;
}

template <typename T>
PyObject* class_call(
  py_class<T>* self,
  PyTupleObject* args,
  PyObject* kwargs
);

template <typename T>
Py_ssize_t class_len(py_class<T>* self) {
  return (*self)->size();
}

template <typename T>
PyObject* class_item(py_class<T>* self, Py_ssize_t i);

template <>
PyObject* class_item(py_class<std::vector<std::string>>* self, Py_ssize_t i) {
  const auto& vec = **self;
  const Py_ssize_t size = vec.size();
  if (i < 0) i = size-i;
  if (i >= size) {
    PyErr_SetString(PyExc_IndexError,
      "index out of range");
    return nullptr;
  }
  const auto& str = vec[i];
  return PyUnicode_FromStringAndSize(str.data(),str.size());
}

template <typename T>
int class_ass_item(py_class<T>* self, Py_ssize_t i, PyObject* v);

template <>
int class_ass_item(
  py_class<std::vector<std::string>>* self, Py_ssize_t i, PyObject* v
) {
  auto& vec = **self;
  const Py_ssize_t size = vec.size();
  if (i < 0) i = size-i;
  if (i >= size) {
    PyErr_SetString(PyExc_IndexError,"index out of range");
    return -1;
  }

  Py_ssize_t len;
  const char* str = PyUnicode_AsUTF8AndSize(v,&len); // sets py exception
  if (!str) return -1;
  vec[i] = { str, (size_t)len };

  return 0;
}

PySequenceMethods string_vector_seq = {
  // https://docs.python.org/3/c-api/typeobj.html#c.PySequenceMethods
  // https://docs.python.org/3/c-api/sequence.html
  // https://docs.python.org/3/c-api/typeobj.html#sequence-structs
  .sq_length = (lenfunc) class_len<std::vector<std::string>>, // sq_length
  // .sq_concat,
  // .sq_repeat,
  .sq_item = (ssizeargfunc) class_item<std::vector<std::string>>,
  .sq_ass_item = (ssizeobjargproc) class_ass_item<std::vector<std::string>>,
  // .sq_contains,
  // .sq_inplace_concat,
  // .sq_inplace_repeat,
};

PyMethodDef string_vector_methods[] = {
  { "front",
    (PyCFunction) +[](py_class<std::vector<std::string>>* self) -> PyObject* {
      const auto& vec = **self;
      if (vec.empty()) {
        PyErr_SetString(PyExc_IndexError,"index 0 is out of range");
        return nullptr;
      }
      const auto& str = vec.front();
      return PyUnicode_FromStringAndSize(str.data(),str.size());
    }, METH_NOARGS,
    "get the first element"
  },
  { }
};

PyTypeObject string_vector_type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  /* tp_name */ STR(MODULE_NAME) ".string_vector",
  /* tp_basicsize */ sizeof(py_class<std::vector<std::string>>),
  /* tp_itemsize */ { },
  /* tp_dealloc */ (destructor) class_dealloc<std::vector<std::string>>,
  /* tp_print */ { },
  /* tp_getattr */ { },
  /* tp_setattr */ { },
  /* tp_as_async */ { },
  /* tp_repr */ { },
  /* tp_as_number */ { },
  /* tp_as_sequence */ &string_vector_seq,
  /* tp_as_mapping */ { },
  /* tp_hash */ { },
  /* tp_call */ { }, // (ternaryfunc) class_call,
  /* tp_str */ { },
  /* tp_getattro */ { },
  /* tp_setattro */ { },
  /* tp_as_buffer */ { },
  /* tp_flags */ Py_TPFLAGS_DEFAULT,
  /* tp_doc */ "python wrapper for std::vector<std::string>>",
  /* tp_traverse */ { },
  /* tp_clear */ { },
  /* tp_richcompare */ { },
  /* tp_weaklistoffset */ { },
  /* tp_iter */ { },
  /* tp_iternext */ { },
  /* tp_methods */ string_vector_methods,
  /* tp_members */ { },
  /* tp_getset */ { },
  /* tp_base */ { },
  /* tp_dict */ { },
  /* tp_descr_get */ { },
  /* tp_descr_set */ { },
  /* tp_dictoffset */ { },
  /* tp_init */ (initproc) class_init<std::vector<std::string>>,
  /* tp_alloc */ { },
  /* tp_new */ PyType_GenericNew,
  /* tp_free */ { },
  /* tp_is_gc */ { },
  /* tp_bases */ { },
  /* tp_mro */ { },
  /* tp_cache */ { },
  /* tp_subclasses */ { },
  /* tp_weaklist */ { },
  /* tp_del */ { },
  /* tp_version_tag */ { },
  /* tp_finalize */ { },
};

PyObject* strlen(PyObject* self, PyObject* arg) {
  Py_ssize_t len;
  const char* str = PyUnicode_AsUTF8AndSize(arg,&len);
  if (!str) {
    PyErr_SetString(PyExc_TypeError,
      "strlen argument must be a string");
    return nullptr;
  }
  return PyLong_FromSsize_t(len);
}

PyMethodDef CAT(MODULE_NAME,_methods)[] = {
  { "strlen", strlen, METH_O,
    "get length of string" },
  { }
};

PyModuleDef CAT(MODULE_NAME,_module) = {
  PyModuleDef_HEAD_INIT,
  /* m_name */ STR(MODULE_NAME),
  /* m_doc */
    "Template for Python module in C++",
  /* m_size */ -1,
  /* m_methods */ CAT(MODULE_NAME,_methods),
  /* m_slots */ { },
  /* m_traverse */ { },
  /* m_clear */ { },
  /* m_free */ { },
};

} // end anonymous namespace

#define MODULE_TYPES \
  F(string_vector)

#define MODULE_TYPES_R \
  F(string_vector)

extern "C"
PyMODINIT_FUNC CAT(PyInit_,MODULE_NAME)(void) {

#define F(X) \
  if (PyType_Ready(&X##_type) < 0) return nullptr;
  MODULE_TYPES
#undef F

  PyObject *m = PyModule_Create(&CAT(MODULE_NAME,_module));
  if (!m) return nullptr;

#define F(X) \
  Py_INCREF(&X##_type); \
  if (PyModule_AddObject( m, #X, (PyObject*) &X##_type ) < 0) \
    goto err_##X;
  MODULE_TYPES
#undef F

  return m;

#define F(X) \
  err_##X: Py_DECREF(&X##_type);
  MODULE_TYPES_R
#undef F

  Py_DECREF(m);
  return nullptr;
}
