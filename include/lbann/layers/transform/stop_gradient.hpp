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

#ifndef STOP_GRADIENT_HPP_INCLUDED
#define STOP_GRADIENT_HPP_INCLUDED

#include "lbann/layers/transform/transform.hpp"

namespace lbann {

/** Layer that blocks back propagation.
 *  This layer's output is identical to its input, but its back
 *  propagation output (i.e. its error signal) is always zero. Compare
 *  with the stop_gradient operation in TensorFlow and Keras. Note
 *  that this means that computed gradients in preceeding layers are
 *  not exact gradients of the objective function.
 */
template <data_layout T_layout, El::Device Dev>
class stop_gradient_layer : public transform_layer {
 public:
  stop_gradient_layer(lbann_comm *comm,
                      cudnn::cudnn_manager* cudnn = nullptr)
    : transform_layer(comm) {
    this->m_cudnn = cudnn;
  }
  stop_gradient_layer* copy() const override { return new stop_gradient_layer(*this); }
  std::string get_type() const override { return "stop_gradient"; }
  data_layout get_data_layout() const override { return T_layout; }
  El::Device get_device_allocation() const override { return Dev; }

  void setup_gpu() override {
    transform_layer::setup_gpu();
#ifdef HYDROGEN_HAVE_CUB
    // Set output matrix to use CUB GPU memory pool
    // Note: During each forward prop, the output matrix is resized to
    // the mini-batch size and cleared to obtain a matrix view. To
    // avoid expensive GPU memory allocation and deallocation, we use
    // CUB's GPU memory pool.
    if (Dev == El::Device::GPU) {
      get_local_activations().SetMemoryMode(1);
    }
#endif
  }

  void fp_compute() override {
    El::LockedView(get_activations(), get_prev_activations());
  }

};

} // namespace lbann

#endif // STOP_GRADIENT_HPP_INCLUDED