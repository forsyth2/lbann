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

#include "lbann/callbacks/callback_ltfb.hpp"
#include "lbann/callbacks/callback_imcomm.hpp"
#include "lbann/utils/random.hpp"
#include <typeinfo>
#include <typeindex>

namespace lbann {

namespace {

/** Generate partner model assignments.
 *
 *  Requires a scatter from the world master process. If there are an
 *  odd number of models, one of them is partnered with itself.
 */
El::Int get_partner_model_index(lbann_comm& comm,
                                const std::string& message_prefix) {
  if (comm.am_world_master()) { // Root process

    // Assign partner models
    // Note: The first model in 'models' is paired with the second,
    // the third with the fourth, and so on. If there are an odd
    // number of models, the last one is partnered with itself.
    const El::Int num_models = comm.get_num_models();
    const El::Int procs_per_model = comm.get_procs_per_model();
    std::vector<El::Int> models(num_models);
    std::iota(models.begin(), models.end(), 0);
    std::shuffle(models.begin(), models.end(), get_fast_generator());

    // Print partner assignments to standard output
    std::stringstream msg;
    msg << message_prefix << "tournament partners -";
    for (El::Int i = 0; i < num_models; i += 2) {
      msg << (i > 0 ? "," : "")
          << " {" << models[i];
      if (i+1 < num_models) {
        msg << "," << models[i+1];
      }
      msg << "}";
    }
    std::cout << msg.str() << std::endl;

    // Send partner assignments to all processes
    std::vector<El::Int> send_buffer(num_models * procs_per_model);
    for (El::Int i = 0; i < num_models; i += 2) {
      const auto& model1 = models[i];
      const auto& model2 = (i+1 < num_models) ? models[i+1] : model1;
      std::fill_n(&send_buffer[model1 * procs_per_model],
                  procs_per_model, model2);
      std::fill_n(&send_buffer[model2 * procs_per_model],
                  procs_per_model, model1);
    }
    return comm.scatter(send_buffer.data(), comm.get_world_comm());

  } else { // Non-root process
    return comm.scatter<int>(comm.get_world_master(),
                             comm.get_world_comm());
  }
}

/** Exchange weights values with partner model.
 *
 *  @param weights_names    Names of weights to exchange. If empty,
 *                          then all weights are exchanged.
 *  @param send_weights     Weights values sent to partner.
 *  @param recv_weights     Weights values recieved from partner.
 */
void exchange_weights(lbann_comm& comm,
                      El::Int partner_model_index,
                      const std::set<std::string>& weights_names,
                      const std::vector<weights*>& send_weights,
                      std::vector<weights*>& recv_weights) {

  // Get partner process
  const El::Int rank_in_model = comm.get_rank_in_model();
  const El::Int procs_per_model = comm.get_procs_per_model();
  const El::Int partner_rank_in_world = (partner_model_index * procs_per_model
                                         + rank_in_model);

  // Exchange weights with partner
  for (size_t i = 0; i < send_weights.size(); ++i) {
    if (!weights_names.empty()
        && (std::find(weights_names.begin(), weights_names.end(),
                      send_weights[i]->get_name())
            == weights_names.end())) {
      continue;
    }
    const auto& send_data = send_weights[i]->get_values().LockedMatrix();
    auto& recv_data = recv_weights[i]->get_values().Matrix();
    El::SendRecv(send_data, recv_data, comm.get_world_comm(),
                 partner_rank_in_world, partner_rank_in_world);
  }

}

/** Get mean metric value with validation set. */
EvalType evaluate(model& m, const std::string& metric_name) {

  // Evaluate model on validation set
  const auto& original_mode = m.get_execution_mode();
  m.evaluate(execution_mode::validation);
  m.set_execution_mode(original_mode);

  // Get metric value
  for (const auto& met : m.get_metrics()) {
    if (met->name() == metric_name) {
      return met->get_mean_value(execution_mode::validation);
    }
  }

  // Error if model does not contain metric
  std::stringstream err;
  err << "could not find metric \"" << metric_name << "\""
      << "in model \"" << m.get_name() << "\"";
  LBANN_ERROR(err.str());
  return EvalType(0);

}

} // namespace

lbann_callback_ltfb::lbann_callback_ltfb(El::Int batch_interval,
                                         std::string metric_name,
                                         std::set<std::string> weights_names,
                                         bool low_score_wins,
                                         lbann_summary *summarizer)
  : lbann_callback(batch_interval, summarizer),
    m_metric_name(std::move(metric_name)),
    m_weights_names(std::move(weights_names)),
    m_low_score_wins(low_score_wins) {}

lbann_callback_ltfb::lbann_callback_ltfb(const lbann_callback_ltfb& other) :
  lbann_callback(other),
  m_metric_name(other.m_metric_name),
  m_weights_names(other.m_weights_names),
  m_low_score_wins(other.m_low_score_wins) {

  // Deep copy
  m_workspace_weights.clear();
  m_workspace_weights.reserve(other.m_workspace_weights.size());
  for (const auto& w : other.m_workspace_weights) {
    m_workspace_weights.emplace_back(w->copy());
  }

}

lbann_callback_ltfb& lbann_callback_ltfb::operator=(const lbann_callback_ltfb& other) {
  lbann_callback::operator=(other);

  // Shallow copies
  m_metric_name = other.m_metric_name;
  m_weights_names = other.m_weights_names;
  m_low_score_wins = other.m_low_score_wins;

  // Deep copy
  m_workspace_weights.clear();
  m_workspace_weights.reserve(other.m_workspace_weights.size());
  for (const auto& w : other.m_workspace_weights) {
    m_workspace_weights.emplace_back(w->copy());
  }

  return *this;
}

void lbann_callback_ltfb::setup(model *m) {

  // Create workspace objects
  const auto& model_weights = m->get_weights();
  m_workspace_weights.clear();
  m_workspace_weights.reserve(model_weights.size());
  for (const auto& w : model_weights) {
    m_workspace_weights.emplace_back(w->copy());
  }

  // Make sure model does not have inter-model communication callback
  for (auto&& cb : m->get_callbacks()) {
    if (dynamic_cast<lbann_callback_imcomm*>(cb) != nullptr) {
      LBANN_ERROR("Detected both LTFB and imcomm callbacks. ");
    }
  }

}

void lbann_callback_ltfb::on_batch_begin(model *m) {
  auto&& comm = m->get_comm();

  // Check whether to start LTFB round
  const auto& mode = m->get_execution_mode();
  const auto& step = m->get_cur_step();
  if (mode != execution_mode::training || step == 0) { return; }
  const auto& message_prefix = (std::string{} + "LTFB ("
                                + "model \"" + m->get_name() + "\", "
                                + "step " + std::to_string(step)
                                + "): ");

  if (comm->am_world_master()) {
    std::cout << message_prefix + "starting tournament..." << std::endl;
  }

  // Determine partner model for tournament
  const El::Int local_model_index = comm->get_model_rank();
  const El::Int partner_model_index = get_partner_model_index(*comm,
                                                              message_prefix);

  // Evaluate local model
  if (comm->am_world_master()) {
    std::cout << message_prefix + "evaluating local model..." << std::endl;
  }
  const auto& local_score = evaluate(*m, m_metric_name);

  // Replace model weights with data from partner model
  /// @todo Use checkpointing
  if (comm->am_world_master()) {
    std::cout << message_prefix + "exchanging weights with partner..." << std::endl;
  }
  auto&& model_weights = m->get_weights();
  std::vector<weights*> local_weights;
  for (size_t i = 0; i < model_weights.size(); ++i) {
    local_weights.push_back(m_workspace_weights[i].get());
    *local_weights[i] = *model_weights[i];
  }
  exchange_weights(*comm, partner_model_index, m_weights_names,
                   local_weights, model_weights);

  // Evaluate partner model
  if (comm->am_world_master()) {
    std::cout << message_prefix + "evaluating partner model..." << std::endl;
  }
  const auto& partner_score = evaluate(*m, m_metric_name);

  // Restore old weights if they achieved a better score
  if ((m_low_score_wins && local_score <= partner_score) ||
      (!m_low_score_wins && local_score >= partner_score)) {
    for (size_t i = 0; i < model_weights.size(); ++i) {
     *model_weights[i] = *local_weights[i];
    }
  } else {
    if (comm->am_model_master()) {
      std::stringstream msg;
      msg << message_prefix
          << "replacing model " << local_model_index << " "
          << "(" << local_score << " score) "
          << "with model " << partner_model_index << " "
          << "(" << partner_score << " score) ";
      std::cout << msg.str() << std::endl;
    }
  }

}

} // namespace lbann
