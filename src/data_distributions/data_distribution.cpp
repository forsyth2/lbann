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

#include "lbann/data_distributions/data_distribution.hpp"
#include "lbann/utils/exception.hpp"

lbann::generic_data_distribution::generic_data_distribution(lbann_comm *comm, int num_parallel_readers, int mini_batch_size, std::map<execution_mode, generic_data_reader *> data_readers)
  : m_comm(comm), m_num_parallel_readers_training(num_parallel_readers), m_num_parallel_readers_validating(num_parallel_readers), m_num_parallel_readers_testing(num_parallel_readers), m_max_mini_batch_size(mini_batch_size), m_data_readers(data_readers) {
  m_root = 0;
  m_num_samples_in_batch = 0;
  m_num_valid_readers = 0;

  m_cur_step_in_epoch = 0;
}

int lbann::generic_data_distribution::get_num_parallel_readers() {
  int num_parallel_readers = 0;
  switch(get_execution_mode()) {
  case execution_mode::training:
    num_parallel_readers = m_num_parallel_readers_training;
    break;
  case execution_mode::validation:
    num_parallel_readers = m_num_parallel_readers_validating;
    break;
  case execution_mode::testing:
    num_parallel_readers = m_num_parallel_readers_testing;
    break;
  default:
    throw lbann_exception(
      std::string{} + __FILE__ + " " + std::to_string(__LINE__) +
      " :: generic data distribution: invalid execution phase");
  }
  return num_parallel_readers;
}

int lbann::generic_data_distribution::get_num_iterations_per_epoch() {
  generic_data_reader *data_reader;
  switch(get_execution_mode()) {
  case execution_mode::training:
    data_reader = m_data_readers[execution_mode::training];
    break;
  case execution_mode::validation:
    data_reader = m_data_readers[execution_mode::validation];
    break;
  case execution_mode::testing:
    data_reader = m_data_readers[execution_mode::testing];
    break;
  default:
    throw lbann_exception(
      std::string{} + __FILE__ + " " + std::to_string(__LINE__) +
      " :: generic data distribution: invalid execution phase");
  }
  return data_reader->get_num_iterations_per_epoch();
}
