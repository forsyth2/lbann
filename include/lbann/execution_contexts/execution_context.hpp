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

#ifndef LBANN_EXECUTION_CONTEXT_HPP
#define LBANN_EXECUTION_CONTEXT_HPP

#include "lbann/base.hpp"
#include "lbann/comm.hpp"
#include "lbann/io/persist.hpp"
#include "lbann/utils/threads/thread_pool.hpp"

namespace lbann {

// Forward-declare this.
class trainer;

struct termination_criteria {
  El::Int num_steps;
};

class execution_context {
public:
  /** Constructor. */
  execution_context(observing_ptr<trainer> trainer, lbann_comm *comm, execution_mode mode);

  /** Copy constructor. */
  execution_context(const execution_context& other) = default;
  /** Copy assignment operator. */
  execution_context& operator=(const execution_context& other) = default;
  /** Move constructor. */
  execution_context(execution_context&& other) = default;
  /** Move assignment operator. */
  execution_context& operator=(execution_context&& other) = default;
  /** Destructor. */
  virtual ~execution_context() = default;
  /** Copy execution_context. */
  //  virtual execution_context* copy() const = default;

  /** @brief Current step in the training algorithm
    *  @detailed Step counts are abstract stages in the training
    *  algorithm's internal state
    */
  virtual El::Int get_step() const noexcept { return m_step; }

  /** @brief Increment the current step in the training algorithm
    *  @detailed Increment the step count in the training
    *  algorithm's internal state
    */
  virtual void inc_step() noexcept { ++m_step; }

  /** Get the mode that the trainer is currenting executing. */
  inline void set_execution_mode(execution_mode mode) noexcept {
    m_execution_mode = mode;
  }

  /** Get the mode that the trainer is currenting executing. */
  inline execution_mode get_execution_mode() const noexcept {
    return m_execution_mode;
  }

  /** Return true if the flag to stop training is set. */
  bool get_terminate_training() const {
    return m_terminate_training;
  }
  /** Set the terminate training flag (on or off). */
  void set_terminate_training(bool f) {
    m_terminate_training = f;
  }

  /** Get the execution environment */
  trainer* get_trainer() {
    return m_trainer;
  }

  observing_ptr<thread_pool> get_io_thread_pool() const;

  /** Are background I/O activities enabled by the input layers */
  bool background_io_activity_allowed();

  /** Checkpoint training_algorithm to given file descriptor, return number of bytes written */
  virtual bool save_to_checkpoint_shared(persist& p);
  /** Restore training_algorithm by reading checkpoint from given file descriptor, return number of bytes read */
  virtual bool load_from_checkpoint_shared(persist& p);
  virtual bool save_to_checkpoint_distributed(persist& p);
  virtual bool load_from_checkpoint_distributed(persist& p);

public:
  /** Pointer to the training context (execution environment) for the training algorithm */
  observing_ptr<trainer> m_trainer;

  /** LBANN communicator. */
  observing_ptr<lbann_comm> m_comm;

  /** The trainer's current execution mode. */
  execution_mode m_execution_mode = execution_mode::training;

  /** @brief Current step in the training algorithm
    *  @detailed Step counts are abstract stages in the training
    *  algorithm's internal state
    */
  El::Int m_step = 0;

  /** @brief Whether to terminate training.
   *  @detailed If true, training will terminate immediately before
   *  the next epoch.
   */
  bool m_terminate_training = false;
};

}  // namespace lbann

#endif  // LBANN_EXECUTION_CONTEXT_HPP