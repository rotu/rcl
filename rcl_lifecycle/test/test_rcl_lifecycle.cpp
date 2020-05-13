// Copyright 2020 Open Source Robotics Foundation, Inc.
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

// testing default transition sequence.
// This test requires that the transitions are set
// as depicted in design.ros2.org

#include <gtest/gtest.h>

#include "rcl_lifecycle/rcl_lifecycle.h"
#include "osrf_testing_tools_cpp/memory_tools/memory_tools.hpp"
#include "osrf_testing_tools_cpp/scope_exit.hpp"
#include "rcl/error_handling.h"
#include "lifecycle_msgs/msg/transition_event.h"
#include "lifecycle_msgs/srv/change_state.h"
#include "lifecycle_msgs/srv/get_available_states.h"
#include "lifecycle_msgs/srv/get_available_transitions.h"
#include "lifecycle_msgs/srv/get_state.h"

TEST(TestRclLifecycle, lifecycle_state) {
  rcl_lifecycle_state_t state = rcl_lifecycle_get_zero_initialized_state();
  EXPECT_EQ(state.id, 0u);
  EXPECT_EQ(state.label, nullptr);

  rcl_allocator_t allocator = rcl_get_default_allocator();
  unsigned int expected_id = 42;
  const char expected_label[] = "label";
  rcl_ret_t ret = rcl_lifecycle_state_init(&state, expected_id, &expected_label[0], nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_init(&state, expected_id, nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_init(nullptr, expected_id, &expected_label[0], &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_init(&state, expected_id, &expected_label[0], &allocator);
  EXPECT_EQ(state.id, expected_id);
  EXPECT_STREQ(state.label, &expected_label[0]);

  ret = rcl_lifecycle_state_fini(&state, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  // Already finalized
  ret = rcl_lifecycle_state_fini(nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  ret = rcl_lifecycle_state_fini(&state, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;
}

TEST(TestRclLifecycle, lifecycle_transition) {
  rcl_lifecycle_transition_t transition = rcl_lifecycle_get_zero_initialized_transition();
  EXPECT_EQ(transition.id, 0u);
  EXPECT_EQ(transition.label, nullptr);
  EXPECT_EQ(transition.start, nullptr);
  EXPECT_EQ(transition.goal, nullptr);

  rcl_allocator_t allocator = rcl_get_default_allocator();

  // These need to be allocated on heap so rcl_lifecycle_transition_fini doesn't free a stack
  // allocated variable
  rcl_lifecycle_state_t * start = reinterpret_cast<rcl_lifecycle_state_t *>(
    allocator.allocate(sizeof(rcl_lifecycle_state_t), allocator.state));
  EXPECT_NE(start, nullptr);
  rcl_lifecycle_state_t * end = reinterpret_cast<rcl_lifecycle_state_t *>(
    allocator.allocate(sizeof(rcl_lifecycle_state_t), allocator.state));
  EXPECT_NE(end, nullptr);
  const char start_label[] = "start";
  const char end_label[] = "end";
  *start = rcl_lifecycle_get_zero_initialized_state();
  *end = rcl_lifecycle_get_zero_initialized_state();

  rcl_ret_t ret = rcl_lifecycle_state_init(start, 0u, &start_label[0], &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  ret = rcl_lifecycle_state_init(end, 1u, &end_label[0], &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;
  unsigned int expected_id = 42;
  const char expected_label[] = "label";

  ret = rcl_lifecycle_transition_init(
    nullptr, expected_id, nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_init(
    &transition, expected_id, nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_init(
    nullptr, expected_id, nullptr, nullptr, nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_init(
    &transition, expected_id, nullptr, nullptr, nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_init(
    &transition, expected_id, &expected_label[0], nullptr, nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_init(
    &transition, expected_id, &expected_label[0], start, nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_init(
    &transition, expected_id, &expected_label[0], start, end, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;
  EXPECT_EQ(transition.id, expected_id);
  EXPECT_STREQ(transition.label, &expected_label[0]);

  ret = rcl_lifecycle_transition_fini(nullptr, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_transition_fini(&transition, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  // Already finalized
  ret = rcl_lifecycle_transition_fini(nullptr, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  ret = rcl_lifecycle_transition_fini(&transition, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;
}

TEST(TestRclLifecycle, state_machine) {
  rcl_lifecycle_state_machine_t state_machine = rcl_lifecycle_get_zero_initialized_state_machine();
  EXPECT_EQ(state_machine.current_state, nullptr);
  EXPECT_EQ(state_machine.transition_map.states, nullptr);
  EXPECT_EQ(state_machine.transition_map.transitions, nullptr);
  EXPECT_EQ(state_machine.transition_map.states_size, 0u);
  EXPECT_EQ(state_machine.transition_map.transitions_size, 0u);

  rcl_node_t node = rcl_get_zero_initialized_node();
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rcl_context_t context = rcl_get_zero_initialized_context();
  rcl_node_options_t options = rcl_node_get_default_options();
  rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
  rcl_ret_t ret = rcl_init_options_init(&init_options, allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  ret = rcl_init(0, nullptr, &init_options, &context);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT(
  {
    ASSERT_EQ(RCL_RET_OK, rcl_shutdown(&context));
    ASSERT_EQ(RCL_RET_OK, rcl_context_fini(&context));
  });

  ret = rcl_node_init(&node, "node", "namespace", &context, &options);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  const rosidl_message_type_support_t * pn =
    ROSIDL_GET_MSG_TYPE_SUPPORT(lifecycle_msgs, msg, TransitionEvent);
  const rosidl_service_type_support_t * cs =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, ChangeState);
  const rosidl_service_type_support_t * gs =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetState);
  const rosidl_service_type_support_t * gas =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetAvailableStates);
  const rosidl_service_type_support_t * gat =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetAvailableTransitions);
  const rosidl_service_type_support_t * gtg =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetAvailableTransitions);

  ret = rcl_lifecycle_state_machine_init(
    nullptr, &node, pn, cs, gs, gas, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();
  ret = rcl_lifecycle_state_machine_init(
    &state_machine, nullptr, pn, cs, gs, gas, gat, gtg, false, &allocator);

  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();
  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, nullptr, cs, gs, gas, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, nullptr, gs, gas, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();
  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, nullptr, gas, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, gs, nullptr, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, gs, gas, nullptr, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, gs, gas, gat, nullptr, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_ERROR);
  rcutils_reset_error();
  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, gs, gas, gat, gtg, false, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, gs, gas, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  // Allocate some memory and initialize states and transitions so is_initialized will pass
  state_machine.transition_map.states_size = 1u;
  state_machine.transition_map.states = reinterpret_cast<rcl_lifecycle_state_t *>(
    allocator.allocate(
      state_machine.transition_map.states_size * sizeof(rcl_lifecycle_state_t),
      allocator.state));
  ASSERT_NE(state_machine.transition_map.states, nullptr);
  state_machine.transition_map.states[0] = rcl_lifecycle_get_zero_initialized_state();

  state_machine.transition_map.transitions_size = 1u;
  state_machine.transition_map.transitions =
    reinterpret_cast<rcl_lifecycle_transition_t *>(allocator.allocate(
      state_machine.transition_map.transitions_size * sizeof(rcl_lifecycle_transition_t),
      allocator.state));
  ASSERT_NE(state_machine.transition_map.transitions, nullptr);
  state_machine.transition_map.transitions[0] = rcl_lifecycle_get_zero_initialized_transition();

  EXPECT_EQ(rcl_lifecycle_state_machine_is_initialized(&state_machine), RCL_RET_OK);

  ret = rcl_lifecycle_state_machine_fini(&state_machine, &node, nullptr);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_state_machine_fini(&state_machine, &node, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;
}

TEST(TestRclLifecycle, state_transitions) {
  rcl_lifecycle_state_machine_t state_machine =
    rcl_lifecycle_get_zero_initialized_state_machine();
  EXPECT_EQ(state_machine.current_state, nullptr);
  EXPECT_EQ(state_machine.transition_map.states, nullptr);
  EXPECT_EQ(state_machine.transition_map.transitions, nullptr);
  EXPECT_EQ(state_machine.transition_map.states_size, 0u);
  EXPECT_EQ(state_machine.transition_map.transitions_size, 0u);

  rcl_node_t node = rcl_get_zero_initialized_node();
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rcl_context_t context = rcl_get_zero_initialized_context();
  rcl_node_options_t options = rcl_node_get_default_options();
  rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
  rcl_ret_t ret = rcl_init_options_init(&init_options, allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  ret = rcl_init(0, nullptr, &init_options, &context);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT(
  {
    ASSERT_EQ(RCL_RET_OK, rcl_shutdown(&context));
    ASSERT_EQ(RCL_RET_OK, rcl_context_fini(&context));
  });

  ret = rcl_node_init(&node, "node", "namespace", &context, &options);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  const rosidl_message_type_support_t * pn =
    ROSIDL_GET_MSG_TYPE_SUPPORT(lifecycle_msgs, msg, TransitionEvent);
  const rosidl_service_type_support_t * cs =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, ChangeState);
  const rosidl_service_type_support_t * gs =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetState);
  const rosidl_service_type_support_t * gas =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetAvailableStates);
  const rosidl_service_type_support_t * gat =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetAvailableTransitions);
  const rosidl_service_type_support_t * gtg =
    ROSIDL_GET_SRV_TYPE_SUPPORT(lifecycle_msgs, srv, GetAvailableTransitions);

  ret = rcl_lifecycle_state_machine_init(
    &state_machine, &node, pn, cs, gs, gas, gat, gtg, false, &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  // Allocate some memory and initialize states and transitions so is_initialized will pass
  state_machine.transition_map.states_size = 1u;
  state_machine.transition_map.states = reinterpret_cast<rcl_lifecycle_state_t *>(
    allocator.allocate(
      state_machine.transition_map.states_size * sizeof(rcl_lifecycle_state_t),
      allocator.state));
  ASSERT_NE(state_machine.transition_map.states, nullptr);
  state_machine.transition_map.states[0] = rcl_lifecycle_get_zero_initialized_state();
  ret = rcl_lifecycle_state_init(&state_machine.transition_map.states[0], 0, "START", &allocator);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;
  rcl_lifecycle_state_t * start = &state_machine.transition_map.states[0];

  state_machine.transition_map.transitions_size = 1u;
  state_machine.transition_map.transitions =
    reinterpret_cast<rcl_lifecycle_transition_t *>(allocator.allocate(
      state_machine.transition_map.transitions_size * sizeof(rcl_lifecycle_transition_t),
      allocator.state));
  ASSERT_NE(state_machine.transition_map.transitions, nullptr);
  state_machine.transition_map.transitions[0] = rcl_lifecycle_get_zero_initialized_transition();
  ret = rcl_lifecycle_transition_init(
    &state_machine.transition_map.transitions[0], 0, "TRANSITION", start, start, &allocator);
  rcl_lifecycle_transition_t * expected_transition = &state_machine.transition_map.transitions[0];

  start->valid_transition_size = 1;
  start->valid_transitions = reinterpret_cast<rcl_lifecycle_transition_t *>(
    allocator.allocate(
      start->valid_transition_size * sizeof(rcl_lifecycle_transition_t),
      allocator.state));
  EXPECT_NE(start->valid_transitions, nullptr) << rcl_get_error_string().str;
  start->valid_transitions[0] = *expected_transition;

  ret = rcl_lifecycle_state_machine_is_initialized(&state_machine);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  const rcl_lifecycle_transition_t * transition = rcl_lifecycle_get_transition_by_id(nullptr, 0);
  EXPECT_EQ(transition, nullptr) << rcl_get_error_string().str;
  rcutils_reset_error();

  state_machine.current_state = start;
  transition = rcl_lifecycle_get_transition_by_id(state_machine.current_state, 0);
  EXPECT_EQ(transition, &start->valid_transitions[0]);

  transition = rcl_lifecycle_get_transition_by_id(state_machine.current_state, 42);
  EXPECT_EQ(transition, nullptr) << rcl_get_error_string().str;
  rcutils_reset_error();

  transition = rcl_lifecycle_get_transition_by_label(state_machine.current_state, "TRANSITION");
  EXPECT_EQ(transition, &start->valid_transitions[0]);

  transition = rcl_lifecycle_get_transition_by_label(state_machine.current_state, "NOT A LABEL");
  EXPECT_EQ(transition, nullptr) << rcl_get_error_string().str;
  rcutils_reset_error();

  ret = rcl_lifecycle_trigger_transition_by_id(nullptr, 0, false);
  EXPECT_EQ(ret, RCL_RET_ERROR);
  rcutils_reset_error();

  ret = rcl_lifecycle_trigger_transition_by_id(&state_machine, 0, false);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  ret = rcl_lifecycle_trigger_transition_by_label(&state_machine, "TRANSITION", true);
  EXPECT_EQ(ret, RCL_RET_OK) << rcl_get_error_string().str;

  rcl_print_state_machine(&state_machine);
  EXPECT_FALSE(rcutils_error_is_set());
}
