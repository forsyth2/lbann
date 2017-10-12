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
//
// cudnn_wrapper .hpp .cpp - cuDNN support - wrapper classes, utility functions
////////////////////////////////////////////////////////////////////////////////

#ifndef CUDNN_WRAPPER_HPP_INCLUDED
#define CUDNN_WRAPPER_HPP_INCLUDED

#include <vector>
#include "lbann/base.hpp"
#include "lbann/comm.hpp"
#include "lbann/utils/exception.hpp"

#ifdef __LIB_CUDNN
#include <cuda.h>
#include <cudnn.h>
#include <cublas_v2.h>
#endif // #ifdef __LIB_CUDNN

// Error utility macros
#ifdef __LIB_CUDNN
#define FORCE_CHECK_CUDA(cuda_call)                                     \
  do {                                                                  \
    const cudaError_t cuda_status = cuda_call;                          \
    if (cuda_status != cudaSuccess) {                                   \
      std::cerr << "CUDA error: " << cudaGetErrorString(cuda_status) << "\n"; \
      std::cerr << "Error at " << __FILE__ << ":" << __LINE__ << "\n";  \
      cudaDeviceReset();                                                \
      throw lbann::lbann_exception("CUDA error");                       \
    }                                                                   \
  } while (0)
#define FORCE_CHECK_CUDNN(cudnn_call)                                   \
  do {                                                                  \
    const cudnnStatus_t cudnn_status = cudnn_call;                      \
    if (cudnn_status != CUDNN_STATUS_SUCCESS) {                         \
      std::cerr << "cuDNN error: " << cudnnGetErrorString(cudnn_status) << "\n"; \
      std::cerr << "Error at " << __FILE__ << ":" << __LINE__ << "\n";  \
      cudaDeviceReset();                                                \
      throw lbann::lbann_exception("cuDNN error");                      \
    }                                                                   \
  } while (0)
#define FORCE_CHECK_CUBLAS(cublas_call)                                 \
  do {                                                                  \
    const cublasStatus_t cublas_status = cublas_call;                   \
    if (cublas_status != CUBLAS_STATUS_SUCCESS) {                       \
      std::cerr << "CUBLAS error";                                      \
      std::cerr << "Error at " << __FILE__ << ":" << __LINE__ << "\n";  \
      cudaDeviceReset();                                                \
      throw lbann::lbann_exception("CUBLAS error");                     \
    }                                                                   \
  } while (0)
#ifdef LBANN_DEBUG
#define CHECK_CUDA(cuda_call)     FORCE_CHECK_CUDA(cuda_call)
#define CHECK_CUDNN(cudnn_call)   FORCE_CHECK_CUDNN(cudnn_call)
#define CHECK_CUBLAS(cublas_call) FORCE_CHECK_CUBLAS(cublas_call)
#else
#define CHECK_CUDA(cuda_call)     cuda_call
#define CHECK_CUDNN(cudnn_call)   cudnn_call
#define CHECK_CUBLAS(cublas_call) cublas_call
#endif // #ifdef LBANN_DEBUG
#endif // #ifdef __LIB_CUDNN

namespace cudnn {

/** cuDNN manager class */
class cudnn_manager {
#ifdef __LIB_CUDNN

 public:
  /** Constructor
   *  @param _comm         Pointer to LBANN communicator
   *  @param max_num_gpus  Maximum Number of available GPUs. If
   *                       negative, then use all available GPUs.
   */
  cudnn_manager(lbann::lbann_comm *_comm, int max_num_gpus = -1);

  /** Destructor */
  ~cudnn_manager();

  /** Print cuDNN version information to standard output. */
  void print_version() const;
  /** Get cuDNN data type associated with C++ data type. */
  cudnnDataType_t get_cudnn_data_type() const;

  /** Get number of GPUs assigned to current process. */
  int get_num_gpus() const;
  /** Get number of GPUs on current node. */
  int get_num_total_gpus() const;
  /** Get GPUs. */
  std::vector<int>& get_gpus();
  /** Get GPUs (const). */
  const std::vector<int>& get_gpus() const;
  /** Get ith GPU. */
  int get_gpu(int i=0) const;
  /** Get CUDA streams. */
  std::vector<cudaStream_t>& get_streams();
  /** Get CUDA streams (const). */
  const std::vector<cudaStream_t>& get_streams() const;
  /** Get ith CUDA stream. */
  cudaStream_t& get_stream(int i=0);
  /** Get ith CUDA stream (const). */
  const cudaStream_t& get_stream(int i=0) const;
  /** Get cuDNN handles. */
  std::vector<cudnnHandle_t>& get_handles();
  /** Get cuDNN handles (const). */
  const std::vector<cudnnHandle_t>& get_handles() const;
  /** Get ith cuDNN handle. */
  cudnnHandle_t& get_handle(int i=0);
  /** Get ith cuDNN handle (const). */
  const cudnnHandle_t& get_handle(int i=0) const;
  /** Get CUBLAS handles. */
  std::vector<cublasHandle_t>& get_cublas_handles();
  /** Get CUBLAS handles (const). */
  const std::vector<cublasHandle_t>& get_cublas_handles() const;
  /** Get ith CUBLAS handle. */
  cublasHandle_t& get_cublas_handle(int i=0);
  /** Get ith CUBLAS handle (const). */
  const cublasHandle_t& get_cublas_handle(int i=0) const;
  /** Get GPU work spaces. */
  std::vector<void*> get_work_spaces();
  /** Get ith GPU work space. */
  void *get_work_space(int i=0);
  /** Get GPU work space sizes (in bytes). */
  std::vector<size_t> get_work_space_sizes();
  /** Get GPU work space sizes (in bytes) (const). */
  const std::vector<size_t> get_work_space_sizes() const;
  /** Get ith GPU work space size (in bytes). */
  size_t get_work_space_size(int i=0) const;
  /** Set ith GPU work space size (in bytes). */
  void set_work_space_size(int i, size_t size);

  /** Allocate memory on GPUs. */
  void allocate_on_gpus(std::vector<DataType*>& gpu_data,
                        int height,
                        int width_per_gpu);
  /** Deallocate memory on GPUs. */
  void deallocate_on_gpus(std::vector<DataType*>& gpu_data);

  /** Zero out memory on GPUs. */
  void clear_on_gpus(std::vector<DataType*>& gpu_data,
                     int height,
                     int width_per_gpu);
  /** Zero out memory corresponding to unused columns on GPUs. */
  void clear_unused_columns_on_gpus(std::vector<DataType*>& gpu_data,
                                    int height,
                                    int width,
                                    int width_per_gpu);

  /** Copy data on GPUs. */
  void copy_on_gpus(std::vector<DataType*>& gpu_dst_data,
                    const std::vector<DataType*>& gpu_src_data,
                    int height,
                    int width_per_gpu,
                    int src_leading_dim = 0,
                    int dst_leading_dim = 0);
  /** Copy data from CPU to GPUs.
   *  Matrix columns are scattered amongst GPUs.
   */
  void scatter_to_gpus(std::vector<DataType*>& gpu_data,
                       const Mat& cpu_data,
                       int width_per_gpu,
                       int gpu_data_leading_dim = 0);
  /** Copy data from GPUs to CPU.
   *  Matrix columns are gathered from GPUs.
   */
  void gather_from_gpus(Mat& cpu_data,
                        const std::vector<DataType*>& gpu_data,
                        int width_per_gpu,
                        int gpu_data_leading_dim = 0);
  /** Copy data from CPU to GPUs.
   *  Data is duplicated across GPUs.
   */
  void broadcast_to_gpus(std::vector<DataType*>& gpu_data,
                         const Mat& cpu_data,
                         int gpu_data_leading_dim = 0);
  /** Copy data from GPUs to CPU and reduce.
   */
  void reduce_from_gpus(Mat& cpu_data,
                        const std::vector<DataType*>& gpu_data,
                        int gpu_data_leading_dim = 0);
  /** Allreduce within local multiple GPUs
   */
  void allreduce(const std::vector<DataType*>& gpu_data,
                 El::Int height,
                 El::Int width);

  /** Synchronize the default stream. */
  void synchronize();

  /** Synchronize all streams. */
  void synchronize_all();

  /** Create copy of GPU data.
   *  The GPU memory allocated in this function must be deallocated
   *  elsewhere.
   */
  std::vector<DataType*> copy(const std::vector<DataType*>& gpu_data,
                              int height,
                              int width_per_gpu,
                              int leading_dim = 0);
  
  /** Pin matrix memory.
   *  Pinned memory accelerates memory transfers with GPU, but may
   *  degrade system performance. This function assumes that the
   *  matrix memory was previously allocated within Elemental.
   */
  void pin_matrix(AbsDistMat& mat);

  /** Unpin matrix memory.
   *  Pinned memory accelerates memory transfers with GPU, but may
   *  degrade system performance.
   */
  void unpin_matrix(AbsDistMat& mat);

  void check_error();

 private:

  /** LBANN communicator. */
  lbann::lbann_comm *comm;

  /** Number of GPUs for current process. */
  int m_num_gpus;
  /** Number of available GPUs. */
  int m_num_total_gpus;

  /** List of GPUs. */
  std::vector<int> m_gpus;
  /** List of CUDA streams. */
  std::vector<cudaStream_t> m_streams;
  /** List of cuDNN handles. */
  std::vector<cudnnHandle_t> m_handles;
  /** List of cuDNN handles. */
  std::vector<cublasHandle_t> m_cublas_handles;

  /** List of GPU work spaces. */
  std::vector<void *> m_work_spaces;
  /** List of GPU work space sizes. */
  std::vector<size_t> m_work_space_sizes;

#endif // #ifdef __LIB_CUDNN
};

#ifdef __LIB_CUDNN

/** Copy cuDNN tensor descriptor. */
void copy_tensor_cudnn_desc(const cudnnTensorDescriptor_t& src,
                            cudnnTensorDescriptor_t& dst);

/** Copy cuDNN convolution kernel descriptor. */
void copy_kernel_cudnn_desc(const cudnnFilterDescriptor_t& src,
                            cudnnFilterDescriptor_t& dst);

/** Copy cuDNN convolution descriptor. */
void copy_convolution_cudnn_desc(const cudnnConvolutionDescriptor_t& src,
                                 cudnnConvolutionDescriptor_t& dst);

/** Copy cuDNN pooling descriptor. */
void copy_pooling_cudnn_desc(const cudnnPoolingDescriptor_t& src,
                             cudnnPoolingDescriptor_t& dst);

/** Copy cuDNN activation descriptor. */
void copy_activation_cudnn_desc(const cudnnActivationDescriptor_t& src,
                                cudnnActivationDescriptor_t& dst);

/** Copy cuDNN local response normalization descriptor. */
void copy_lrn_cudnn_desc(const cudnnLRNDescriptor_t& src,
                         cudnnLRNDescriptor_t& dst);

#endif // #ifdef __LIB_CUDNN

}

#endif // CUDNN_WRAPPER_HPP_INCLUDED
