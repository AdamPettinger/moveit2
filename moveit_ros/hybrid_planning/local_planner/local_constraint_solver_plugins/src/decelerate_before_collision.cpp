/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2020, PickNik Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of PickNik Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Sebastian Jahr
 */

#include <moveit/local_constraint_solver_plugins/decelerate_before_collision.h>
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/robot_state/conversions.h>

namespace moveit_hybrid_planning
{
const rclcpp::Logger LOGGER = rclcpp::get_logger("local_planner_component");
const double CYLCE_TIME = 0.01;  // TODO(sjahr) Add param and proper time handling

DecelerateBeforeCollision::DecelerateBeforeCollision() : loop_rate_(1 / CYLCE_TIME){};

bool DecelerateBeforeCollision::initialize(const rclcpp::Node::SharedPtr& node,
                                           const planning_scene_monitor::PlanningSceneMonitorPtr& planning_scene_monitor,
                                           const std::string& group_name)
{
  planning_scene_monitor_ = planning_scene_monitor;
  node_handle_ = node;
  path_invalidation_event_send_ = false;

  // Initialize PID
  const auto jmg = planning_scene_monitor_->getRobotModel()->getJointModelGroup(group_name);
  for (const auto& joint_name : jmg->getActiveJointModelNames())
  {
    joint_position_pids_[joint_name] = control_toolbox::Pid(pid_config_.k_p, pid_config_.k_i, pid_config_.k_d,
                                                            pid_config_.windup_limit, -pid_config_.windup_limit,
                                                            true);  // TODO(sjahr) Add ROS2 param
  }
  return true;
}

moveit_msgs::action::LocalPlanner::Feedback
DecelerateBeforeCollision::solve(const robot_trajectory::RobotTrajectory& local_trajectory,
                                 const std::vector<moveit_msgs::msg::Constraints>& local_constraints,
                                 trajectory_msgs::msg::JointTrajectory& local_solution)
{
  // Feedback result
  moveit_msgs::action::LocalPlanner::Feedback feedback_result;

  // Get current planning scene
  planning_scene_monitor_->updateFrameTransforms();

  planning_scene_monitor::LockedPlanningSceneRO locked_planning_scene(planning_scene_monitor_);

  robot_trajectory::RobotTrajectory robot_command(local_trajectory.getRobotModel(), local_trajectory.getGroupName());
  std::vector<std::size_t>* invalid_index = nullptr;

  // Get Current State
  const moveit::core::RobotState& current_state = locked_planning_scene->getCurrentState();

  // Check if path is valid
  if (locked_planning_scene->isPathValid(local_trajectory, local_trajectory.getGroupName(), false, invalid_index))
  {
    if (path_invalidation_event_send_)
    {
      path_invalidation_event_send_ = false;  // Reset flag
    }

    // Forward next waypoint to the robot controller
    robot_command.addSuffixWayPoint(local_trajectory.getWayPoint(0), 0.0);
  }
  else
  {
    if (!path_invalidation_event_send_)
    {  // Send feedback only once
      feedback_result.feedback = "collision_ahead";
      path_invalidation_event_send_ = true;  // Set feedback flag
    }

    // Keep current position
    robot_command.addSuffixWayPoint(current_state, 0.0);
  }

  // Transform robot trajectory into joint_trajectory message
  moveit_msgs::msg::RobotTrajectory robot_command_msg;
  robot_command.getRobotTrajectoryMsg(robot_command_msg);
  const auto& joint_trajectory = robot_command_msg.joint_trajectory;

  // Transform current state into msg
  moveit_msgs::msg::RobotState current_state_msg;
  robotStateToRobotStateMsg(current_state, current_state_msg);

  // Initialize command goal
  trajectory_msgs::msg::JointTrajectoryPoint command_goal_point;
  command_goal_point.time_from_start = rclcpp::Duration(loop_rate_.period().count());

  // Calculate PID command
  const auto& target_state = joint_trajectory.points[0];
  for (std::size_t i = 0; i < target_state.positions.size(); i++)
  {
    // Skip position if joint is not part of active group
    const auto& joint_name = joint_trajectory.joint_names[i];
    if (joint_position_pids_.find(joint_name) == joint_position_pids_.end())
      continue;

    // Calculate PID command
    const double error = target_state.positions[i] - current_state_msg.joint_state.position[i];
    const double delta_theta = joint_position_pids_[joint_name].computeCommand(error, loop_rate_.period().count());

    // Apply delta to current state to compute goal command
    command_goal_point.positions.push_back(current_state_msg.joint_state.position[i] += delta_theta);
  }

  // Replace local trajectory with goal command
  local_solution.header.stamp = node_handle_->get_clock()->now();
  local_solution.joint_names = joint_trajectory.joint_names;
  local_solution.points = { command_goal_point };

  // Return feedback_result
  return feedback_result;
}
}  // namespace moveit_hybrid_planning

#include <pluginlib/class_list_macros.hpp>

PLUGINLIB_EXPORT_CLASS(moveit_hybrid_planning::DecelerateBeforeCollision,
                       moveit_hybrid_planning::LocalConstraintSolverInterface);