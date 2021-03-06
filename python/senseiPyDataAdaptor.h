#ifndef senseiPyDataAdaptor_h
#define senseiPyDataAdaptor_h

#include "senseiPyObject.h"
#include "senseiPyGILState.h"
#include "Error.h"

#include <Python.h>

#include <vtkDataObject.h>
#include <vtkPythonUtil.h>
#include <string>

// we are going to be overly verbose in an effort to help
// the user debug their code. package this up for use in all
// the callbacks.
#define SENSEI_PY_CALLBACK_ERROR(_method, _cb_obj)        \
  {                                                       \
  PyObject *cb_str = PyObject_Str(_cb_obj);               \
  const char *cb_c_str = PyString_AsString(cb_str);       \
                                                          \
  SENSEI_ERROR("An exception ocurred when invoking the "  \
  "user supplied Python callback \"" << cb_c_str << "\""  \
  "for DataAdaptor::" #_method ". The exception that "    \
  " occurred is:")                                        \
                                                          \
  PyErr_Print();                                          \
                                                          \
  Py_XDECREF(cb_str);                                     \
  }
#include <iostream>
using std::cerr;
using std::endl;

namespace senseiPyDataAdaptor
{

// a container for the DataAdaptor::GetNumberOfMeshes callable
class PyGetNumberOfMeshesCallback
{
public:
  PyGetNumberOfMeshesCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()(unsigned int &numberOfMeshes)
    {
    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A GetNumberOfMeshesCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject *ret = nullptr;
    if (!(ret = PyObject_CallObject(f, nullptr)) || PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(GetNumberOfMeshesCallback, f)
      return -1;
      }

    // convert the return
    if (!senseiPyObject::CppTT<int>::IsType(ret))
      {
      PyErr_Format(PyExc_TypeError,
        "Bad return type from GetNumberOfMeshesCallback (not an int)");
      return -1;
      }

    numberOfMeshes = senseiPyObject::CppTT<int>::Value(ret);
    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

// a container for the DataAdaptor::GetMeshName callable
class PyGetMeshNameCallback
{
public:
  PyGetMeshNameCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()(unsigned int index, std::string &meshName)
    {
    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A GetMeshNameCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject *args = Py_BuildValue("(I)", index);

    PyObject *ret = nullptr;
    if (!(ret = PyObject_CallObject(f, args)) || PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(GetMeshNameCallback, f)
      return -1;
      }

    Py_DECREF(args);

    // convert the return
    if (!senseiPyObject::CppTT<char*>::IsType(ret))
      {
      PyErr_Format(PyExc_TypeError,
        "Bad return type from GetMeshNameCallback");
      return -1;
      }

    meshName = senseiPyObject::CppTT<char*>::Value(ret);
    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

// a container for the DataAdaptor::GetMesh callable
class PyGetMeshCallback
{
public:
  PyGetMeshCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()(const std::string &meshName,
    bool structureOnly, vtkDataObject *&mesh)
    {
    mesh = nullptr;

    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A GetMeshCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject *args = Py_BuildValue("(si)",
      meshName.c_str(), static_cast<int>(structureOnly));

    PyObject *ret = nullptr;
    if (!(ret = PyObject_CallObject(f, args)) || PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(GetMeshCallback, f)
      return -1;
      }

    Py_DECREF(args);

    // convert the return
    mesh = static_cast<vtkDataObject*>(
        vtkPythonUtil::GetPointerFromObject(ret, "vtkDataObject"));

    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

// a container for the DataAdaptor::AddArray callable
class PyAddArrayCallback
{
public:
  PyAddArrayCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()(vtkDataObject* mesh, const std::string &meshName,
    int association, const std::string &arrayName)
    {
    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A AddArrayCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject *pyMesh = vtkPythonUtil::GetObjectFromPointer(
      static_cast<vtkObjectBase*>(mesh));

    PyObject *args = Py_BuildValue("Nsis", pyMesh, meshName.c_str(),
      association, arrayName.c_str());

    PyObject *ret = nullptr;
    if (!(ret = PyObject_CallObject(f, args)) || PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(AddArrayCallback, f)
      return -1;
      }

    Py_DECREF(args);

    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

// a container for the DataAdaptor::GetNumberOfArrays callable
class PyGetNumberOfArraysCallback
{
public:
  PyGetNumberOfArraysCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()(const std::string &meshName, int association,
    unsigned int &numberOfArrays)
    {
    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A GetNumberOfArraysCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject *args = Py_BuildValue("si", meshName.c_str(), association);

    PyObject *ret = nullptr;
    if (!(ret = PyObject_CallObject(f, args)) || PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(GetNumberOfArraysCallback, f)
      return -1;
      }

    Py_DECREF(args);

    // convert the return
    if (!senseiPyObject::CppTT<int>::IsType(ret))
      {
      PyErr_Format(PyExc_TypeError,
        "Bad return type from GetNumberOfArraysCallback (not an int)");
      return -1;
      }

    numberOfArrays = senseiPyObject::CppTT<int>::Value(ret);
    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

// a container for the DataAdaptor::GetArrayName callable
class PyGetArrayNameCallback
{
public:
  PyGetArrayNameCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()(const std::string &meshName, int association,
    unsigned int index, std::string &arrayName)
    {
    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A GetArrayNameCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject *args =
      Py_BuildValue("siI", meshName.c_str(), association, index);

    PyObject *ret = nullptr;
    if (!(ret = PyObject_CallObject(f, args)) || PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(GetArrayNameCallback, f)
      return -1;
      }

    Py_DECREF(args);

    // convert the return
    if (!senseiPyObject::CppTT<char*>::IsType(ret))
      {
      PyErr_Format(PyExc_TypeError,
        "Bad return type from GetArrayNameCallback");
      return -1;
      }

    arrayName = senseiPyObject::CppTT<char*>::Value(ret);
    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

// a container for the DataAdaptor::ReleaseData callable
class PyReleaseDataCallback
{
public:
  PyReleaseDataCallback(PyObject *f) : Callback(f) {}

  void SetObject(PyObject *f)
  { this->Callback.SetObject(f); }

  explicit operator bool() const
  { return static_cast<bool>(this->Callback); }

  int operator()()
    {
    // lock the GIL
    senseiPyGILState gil;

    // get the callback
    PyObject *f = this->Callback.GetObject();
    if (!f)
      {
      PyErr_Format(PyExc_TypeError,
        "A ReleaseDataCallback was not provided");
      return -1;
      }

    // build arguments list and call the callback
    PyObject_CallObject(f, nullptr);
    if (PyErr_Occurred())
      {
      SENSEI_PY_CALLBACK_ERROR(ReleaseDataCallback, f)
      }

    return 0;
    }

private:
  senseiPyObject::PyCallablePointer Callback;
};

}

#endif
