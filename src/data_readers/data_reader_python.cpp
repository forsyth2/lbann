////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_readers/data_reader_python.hpp"
#ifdef LBANN_HAS_PYTHON
#include <cstdio>
#include <algorithm>

namespace lbann {

namespace python {

// Static variables
std::unique_ptr<manager> manager::m_instance;

manager& manager::get_instance() {
  if (m_instance == nullptr) { create(); }
  return *m_instance;
}

void manager::create() {
  m_instance.reset(new manager());
}

void manager::destroy() {
  m_instance.reset(nullptr);
}

manager::manager() {
  if (!Py_IsInitialized()) {

    // Hack to display output from Python
    // Note: Python outputs didn't appear because MPI intercepts
    // stdout and stderr. See
    // https://stackoverflow.com/questions/29352485/python-print-not-working-when-embedded-into-mpi-program
    Py_UnbufferedStdioFlag = 1;

    // Initialize embedded Python session
    Py_Initialize();
    PyEval_InitThreads();

    // Release GIL
    m_thread_state = PyEval_SaveThread();

  }
  if (!Py_IsInitialized()) {
    LBANN_ERROR("error creating embedded Python session");
  }
}

manager::~manager() {
  if (Py_IsInitialized()) {
    if (m_thread_state != nullptr) {
      PyEval_RestoreThread(m_thread_state);
    }
    Py_Finalize();
  }
}

void manager::check_error(bool force_error) const {
  global_interpreter_lock gil(*this);
  if (force_error || PyErr_Occurred()) {

    // Get error information from Python session
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);

    // Construct error message
    std::ostringstream err;
    err << "detected Python error";
    if (value != nullptr) {
      auto msg = PyObject_Repr(value);
      auto msg_str = PyUnicode_AsEncodedString(msg, "utf-8", "Error -");
      err << " (" << PyBytes_AS_STRING(msg_str) << ")";
      Py_XDECREF(msg_str);
      Py_XDECREF(msg);
    }

    // Print Python traceback if available
    if (traceback != nullptr) {

      // Format traceback
      auto module = PyImport_ImportModule("traceback");
      auto func = PyObject_GetAttrString(module, "format_tb");
      auto args = PyTuple_Pack(1, traceback);
      auto message = PyObject_CallObject(func, args);

      // Print traceback
      err << "\n\n" << "Python traceback:";
      auto iter = PyObject_GetIter(message);
      for (auto line = PyIter_Next(iter);
           line != nullptr;
           line = PyIter_Next(iter)) {
        const char* line_ = PyUnicode_AsUTF8(line);
        err << "\n" << (line_ ? line_ : "");
        Py_DECREF(line);
      }

      // Clean up
      Py_XDECREF(iter);
      Py_XDECREF(message);
      Py_XDECREF(args);
      Py_XDECREF(func);
      Py_XDECREF(module);

    }

    // Clean up and throw exception
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    LBANN_ERROR(err.str());

  }
}

global_interpreter_lock::global_interpreter_lock(const manager&)
  : m_gil_state(PyGILState_Ensure()) {}

global_interpreter_lock::~global_interpreter_lock() {
  if (Py_IsInitialized()) {
    PyGILState_Release(m_gil_state);
  }
}

object::object(PyObject* ptr) : m_ptr(ptr) {
  if (Py_IsInitialized() && PyErr_Occurred()) {
    manager::get_instance().check_error();
  }
}

object::object(std::string val)
  : object(PyUnicode_FromStringAndSize(val.c_str(), val.size())) {}
object::object(El::Int val) : object(PyLong_FromLong(val)) {}
object::object(DataType val) : object(PyFloat_FromDouble(val)) {}

object::object(const object& other) : m_ptr(other.m_ptr) {
  Py_XINCREF(m_ptr);
}

object& object::operator=(const object& other) {
  Py_XDECREF(m_ptr);
  m_ptr = other.m_ptr;
  Py_XINCREF(m_ptr);
  return *this;
}

object::object(object&& other) : m_ptr(other.m_ptr) {
  other.m_ptr = nullptr;
}

object& object::operator=(object&& other) {
  Py_XDECREF(m_ptr);
  m_ptr = other.m_ptr;
  other.m_ptr = nullptr;
  return *this;
}

object::~object() {
  if (Py_IsInitialized()) {
    Py_XDECREF(m_ptr);
  }
}

} // namespace python

python_reader::python_reader(std::string module,
                             std::string module_dir,
                             std::string sample_function,
                             std::string num_samples_function,
                             std::string sample_dims_function)
  : generic_data_reader(true) {

  // Acquire Python GIL
  auto& manager = python::manager::get_instance();
  python::global_interpreter_lock gil(manager);

  // Import Python module for data
  if (!module_dir.empty()) {
    auto path = PySys_GetObject("path");  // Borrowed reference
    PyList_Append(path, python::object(module_dir));
    manager.check_error();
  }
  python::object data_module = PyImport_ImportModule(module.c_str());

  // Get number of samples
  python::object num_func
    = PyObject_GetAttrString(data_module, num_samples_function.c_str());
  python::object num = PyObject_CallObject(num_func, nullptr);
  m_num_samples = PyLong_AsLong(num);
  manager.check_error();

  // Get sample dimensions
  python::object dims_func
    = PyObject_GetAttrString(data_module, sample_dims_function.c_str());
  python::object dims = PyObject_CallObject(dims_func, nullptr);
  dims = PyObject_GetIter(dims);
  for (auto d = PyIter_Next(dims); d != nullptr; d = PyIter_Next(dims)) {
    m_sample_dims.push_back(PyLong_AsLong(d));
    Py_DECREF(d);
  }
  manager.check_error();

  // Get sample function
  m_sample_function = PyObject_GetAttrString(data_module,
                                             sample_function.c_str());

  // Initialize Python process pool
  El::Int num_procs = 1;
  if (m_io_thread_pool != nullptr) {
    num_procs = m_io_thread_pool->get_num_threads();
  } else if (!m_thread_buffer.empty()) {
    num_procs = m_thread_buffer.size();
  }
  python::object multiprocessing_module
    = PyImport_ImportModule("multiprocessing");
  m_process_pool = PyObject_CallMethod(multiprocessing_module, "Pool",
                                       "(L)", num_procs);

}

const std::vector<int> python_reader::get_data_dims() const {
  std::vector<int> dims;
  for (const auto& d : m_sample_dims) {
    dims.push_back(d);
  }
  return dims;
}
int python_reader::get_num_labels() const {
  return 1;
}
int python_reader::get_linearized_data_size() const {
  const auto& dims = get_data_dims();
  return std::accumulate(dims.begin(), dims.end(), 1,
                         std::multiplies<int>());
}
int python_reader::get_linearized_label_size() const {
  return get_num_labels();
}

bool python_reader::fetch_datum(CPUMat& X, int data_id, int col) {

  // Acquire Python GIL
  auto& manager = python::manager::get_instance();
  python::global_interpreter_lock gil(manager);

  // Get sample with Python
  // Note: The actual computation is dispatched to a process pool.
  python::object args = PyTuple_New(2);
  python::object sample = PyObject_CallMethod(m_process_pool,
                                              "apply",
                                              "(O,O)",
                                              m_sample_function.get(),
                                              Py_BuildValue("(L)", data_id));


  // Extract sample entries from Python iterator
  sample = PyObject_GetIter(sample);
  const El::Int sample_size = get_linearized_data_size();
  for (El::Int row = 0; row < sample_size; ++row) {
    python::object val = PyIter_Next(sample);
    X(row, col) = PyFloat_AsDouble(val);
  }
  manager.check_error();

  return true;
}

bool python_reader::fetch_label(CPUMat& Y, int data_id, int col) {
  return true;
}

void python_reader::load() {
  m_shuffled_indices.resize(m_num_samples);
  std::iota(m_shuffled_indices.begin(), m_shuffled_indices.end(), 0);
  select_subset_of_data();
}

} // namespace lbann

#endif // LBANN_HAS_PYTHON
