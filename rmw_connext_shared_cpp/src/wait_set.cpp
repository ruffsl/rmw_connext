// Copyright 2015-2017 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rmw_connext_shared_cpp/shared_functions.hpp"

rmw_wait_set_t *
create_wait_set(const char * implementation_identifier, size_t max_conditions)
{
  rmw_wait_set_t * wait_set = rmw_wait_set_allocate();

  ConnextWaitSetInfo * wait_set_info = nullptr;

  // From here onward, error results in unrolling in the goto fail block.
  if (!wait_set) {
    RMW_SET_ERROR_MSG("failed to allocate wait set");
    goto fail;
  }
  wait_set->implementation_identifier = implementation_identifier;
  wait_set->data = rmw_allocate(sizeof(ConnextWaitSetInfo));
  wait_set_info = static_cast<ConnextWaitSetInfo *>(wait_set->data);

  if (!wait_set_info) {
    RMW_SET_ERROR_MSG("failed to allocate wait set");
    goto fail;
  }

  wait_set_info->wait_set = static_cast<DDSWaitSet *>(rmw_allocate(sizeof(DDSWaitSet)));
  if (!wait_set_info->wait_set) {
    RMW_SET_ERROR_MSG("failed to allocate wait set");
    goto fail;
  }

  RMW_TRY_PLACEMENT_NEW(
    wait_set_info->wait_set, wait_set_info->wait_set, goto fail, DDSWaitSet, )

  // Now allocate storage for the ConditionSeq objects
  wait_set_info->active_conditions =
    static_cast<DDSConditionSeq *>(rmw_allocate(sizeof(DDSConditionSeq)));
  if (!wait_set_info->active_conditions) {
    RMW_SET_ERROR_MSG("failed to allocate active_conditions sequence");
    goto fail;
  }

  wait_set_info->attached_conditions =
    static_cast<DDSConditionSeq *>(rmw_allocate(sizeof(DDSConditionSeq)));
  if (!wait_set_info->attached_conditions) {
    RMW_SET_ERROR_MSG("failed to allocate attached_conditions sequence");
    goto fail;
  }

  // If max_conditions is greater than zero, re-allocate both ConditionSeqs to max_conditions
  if (max_conditions > 0) {
    RMW_TRY_PLACEMENT_NEW(
      wait_set_info->active_conditions, wait_set_info->active_conditions, goto fail,
      DDSConditionSeq, static_cast<DDS_Long>(max_conditions))

    RMW_TRY_PLACEMENT_NEW(
      wait_set_info->attached_conditions, wait_set_info->attached_conditions, goto fail,
      DDSConditionSeq,
      static_cast<DDS_Long>(max_conditions))
  } else {
    // Else, don't preallocate: the vectors will size dynamically when rmw_wait is called.
    // Default-construct the ConditionSeqs.
    RMW_TRY_PLACEMENT_NEW(
      wait_set_info->active_conditions, wait_set_info->active_conditions,
      goto fail, DDSConditionSeq, )

    RMW_TRY_PLACEMENT_NEW(
      wait_set_info->attached_conditions, wait_set_info->attached_conditions, goto fail,
      DDSConditionSeq, )
  }

  return wait_set;

fail:
  if (wait_set_info) {
    if (wait_set_info->active_conditions) {
      // How to know which constructor threw?
      RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
        wait_set_info->active_conditions->~DDSConditionSeq(), DDSConditionSeq)
      rmw_free(wait_set_info->active_conditions);
    }
    if (wait_set_info->attached_conditions) {
      RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
        wait_set_info->attached_conditions->~DDSConditionSeq(), DDSConditionSeq)
      rmw_free(wait_set_info->attached_conditions);
    }
    if (wait_set_info->wait_set) {
      RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(wait_set_info->wait_set->~DDSWaitSet(), DDSWaitSet)
      rmw_free(wait_set_info->wait_set);
    }
    wait_set_info = nullptr;
  }
  if (wait_set) {
    if (wait_set->data) {
      rmw_free(wait_set->data);
    }
    rmw_wait_set_free(wait_set);
  }
  return nullptr;
}

rmw_ret_t
destroy_wait_set(const char * implementation_identifier, rmw_wait_set_t * wait_set)
{
  if (!wait_set) {
    RMW_SET_ERROR_MSG("wait set handle is null");
    return RMW_RET_ERROR;
  }
  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    wait_set handle,
    wait_set->implementation_identifier, implementation_identifier,
    return RMW_RET_ERROR)

  auto result = RMW_RET_OK;
  ConnextWaitSetInfo * wait_set_info = static_cast<ConnextWaitSetInfo *>(wait_set->data);

  // Explicitly call destructor since the "placement new" was used
  if (wait_set_info->active_conditions) {
    RMW_TRY_DESTRUCTOR(
      wait_set_info->active_conditions->~DDSConditionSeq(), ConditionSeq, result = RMW_RET_ERROR)
    rmw_free(wait_set_info->active_conditions);
  }
  if (wait_set_info->attached_conditions) {
    RMW_TRY_DESTRUCTOR(
      wait_set_info->attached_conditions->~DDSConditionSeq(), ConditionSeq, result = RMW_RET_ERROR)
    rmw_free(wait_set_info->attached_conditions);
  }
  if (wait_set_info->wait_set) {
    RMW_TRY_DESTRUCTOR(wait_set_info->wait_set->~DDSWaitSet(), WaitSet, result = RMW_RET_ERROR)
    rmw_free(wait_set_info->wait_set);
  }
  wait_set_info = nullptr;
  if (wait_set->data) {
    rmw_free(wait_set->data);
  }
  if (wait_set) {
    rmw_wait_set_free(wait_set);
  }
  return result;
}
