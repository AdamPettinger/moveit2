/*******************************************************************************
 *      Title     : servo_calcs.h
 *      Project   : moveit_servo
 *      Created   : 1/11/2019
 *      Author    : Brian O'Neil, Andy Zelenak, Blake Anderson
 *
 * BSD 3-Clause License
 *
 * Copyright (c) 2019, Los Alamos National Security, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

#pragma once

// ROS
#include <rclcpp/rclcpp.hpp>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
#include <moveit/robot_model_loader/robot_model_loader.h>
#include <moveit_msgs/srv/change_drift_dimensions.hpp>
#include <moveit_msgs/srv/change_control_dimensions.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/int8.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <control_msgs/msg/joint_jog.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <trajectory_msgs/msg/joint_trajectory.hpp>

// moveit_servo
#include <moveit_servo/servo_parameters.h>
#include <moveit_servo/status_codes.h>
#include <moveit_servo/low_pass_filter.h>

namespace moveit_servo
{
class ServoCalcs : public rclcpp::Node
{
public:
  ServoCalcs(const rclcpp::NodeOptions& options, const ServoParameters& parameters,
             const planning_scene_monitor::PlanningSceneMonitorPtr& planning_scene_monitor);

  /** \brief Start and stop the timer where we do work and publish outputs */
  void start();
  void stop();

  /**
   * Get the MoveIt planning link transform.
   * The transform from the MoveIt planning frame to robot_link_command_frame
   *
   * @param transform the transform that will be calculated
   * @return true if a valid transform was available
   */
  bool getCommandFrameTransform(Eigen::Isometry3d& transform);

  /** \brief Pause or unpause processing servo commands while keeping the timers alive */
  void setPaused(bool paused);

  /** \brief Get the latest joint state */
  sensor_msgs::msg::JointState::ConstSharedPtr getLatestJointState() const;

private:
  /** \brief Timer method */
  void run();  // TODO(adamp): come back and pass a timer event here?

  /** \brief Do servoing calculations for Cartesian twist commands. */
  bool cartesianServoCalcs(geometry_msgs::msg::TwistStamped& cmd,
                           trajectory_msgs::msg::JointTrajectory& joint_trajectory);

  /** \brief Do servoing calculations for direct commands to a joint. */
  bool jointServoCalcs(const control_msgs::msg::JointJog& cmd, trajectory_msgs::msg::JointTrajectory& joint_trajectory);

  /** \brief Parse the incoming joint msg for the joints of our MoveGroup */
  bool updateJoints();

  /** \brief If incoming velocity commands are from a unitless joystick, scale them to physical units.
   * Also, multiply by timestep to calculate a position change.
   */
  Eigen::VectorXd scaleCartesianCommand(const geometry_msgs::msg::TwistStamped& command);

  /** \brief If incoming velocity commands are from a unitless joystick, scale them to physical units.
   * Also, multiply by timestep to calculate a position change.
   */
  Eigen::VectorXd scaleJointCommand(const control_msgs::msg::JointJog& command);

  bool addJointIncrements(sensor_msgs::msg::JointState& output, const Eigen::VectorXd& increments);

  /** \brief Suddenly halt for a joint limit or other critical issue.
   * Is handled differently for position vs. velocity control.
   */
  void suddenHalt(trajectory_msgs::msg::JointTrajectory& joint_trajectory) const;

  /** \brief  Scale the delta theta to match joint velocity/acceleration limits */
  void enforceSRDFAccelVelLimits(Eigen::ArrayXd& delta_theta);

  /** \brief Avoid overshooting joint limits */
  bool enforceSRDFPositionLimits();

  /** \brief Possibly calculate a velocity scaling factor, due to proximity of
   * singularity and direction of motion
   */
  double velocityScalingFactorForSingularity(const Eigen::VectorXd& commanded_velocity,
                                             const Eigen::JacobiSVD<Eigen::MatrixXd>& svd,
                                             const Eigen::MatrixXd& pseudo_inverse);

  /**
   * Slow motion down if close to singularity or collision.
   * @param delta_theta motion command, used in calculating new_joint_tray
   * @param singularity_scale tells how close we are to a singularity
   */
  void applyVelocityScaling(Eigen::ArrayXd& delta_theta, double singularity_scale);

  /** \brief Compose the outgoing JointTrajectory message */
  void composeJointTrajMessage(const sensor_msgs::msg::JointState& joint_state,
                               trajectory_msgs::msg::JointTrajectory& joint_trajectory);

  /** \brief Smooth position commands with a lowpass filter */
  void lowPassFilterPositions(sensor_msgs::msg::JointState& joint_state);

  /** \brief Set the filters to the specified values */
  void resetLowPassFilters(const sensor_msgs::msg::JointState& joint_state);

  /** \brief Convert an incremental position command to joint velocity message */
  void calculateJointVelocities(sensor_msgs::msg::JointState& joint_state, const Eigen::ArrayXd& delta_theta);

  /** \brief Convert joint deltas to an outgoing JointTrajectory command.
   * This happens for joint commands and Cartesian commands.
   */
  bool convertDeltasToOutgoingCmd(trajectory_msgs::msg::JointTrajectory& joint_trajectory);

  /** \brief Gazebo simulations have very strict message timestamp requirements.
   * Satisfy Gazebo by stuffing multiple messages into one.
   */
  void insertRedundantPointsIntoTrajectory(trajectory_msgs::msg::JointTrajectory& joint_trajectory, int count) const;

  /**
   * Remove the Jacobian row and the delta-x element of one Cartesian dimension, to take advantage of task redundancy
   *
   * @param matrix The Jacobian matrix.
   * @param delta_x Vector of Cartesian delta commands, should be the same size as matrix.rows()
   * @param row_to_remove Dimension that will be allowed to drift, e.g. row_to_remove = 2 allows z-translation drift.
   */
  void removeDimension(Eigen::MatrixXd& matrix, Eigen::VectorXd& delta_x, unsigned int row_to_remove) const;

  /* \brief Callback for joint subsription */
  void jointStateCB(const sensor_msgs::msg::JointState::SharedPtr msg);

  /* \brief Command callbacks */
  void twistStampedCB(const geometry_msgs::msg::TwistStamped::SharedPtr msg);
  void jointCmdCB(const control_msgs::msg::JointJog::SharedPtr msg);
  void collisionVelocityScaleCB(const std_msgs::msg::Float64::SharedPtr msg);

  /**
   * Allow drift in certain dimensions. For example, may allow the wrist to rotate freely.
   * This can help avoid singularities.
   *
   * @param request the service request
   * @param response the service response
   * @return true if the adjustment was made
   */
  void changeDriftDimensions(const std::shared_ptr<moveit_msgs::srv::ChangeDriftDimensions::Request> req,
                             std::shared_ptr<moveit_msgs::srv::ChangeDriftDimensions::Response> res);

  /** \brief Start the main calculation timer */
  // Service callback for changing servoing dimensions
  void changeControlDimensions(const std::shared_ptr<moveit_msgs::srv::ChangeControlDimensions::Request> req,
                               std::shared_ptr<moveit_msgs::srv::ChangeControlDimensions::Response> res);

  // Parameters from yaml
  const ServoParameters& parameters_;

  // Pointer to the collision environment
  planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor_;

  // Track the number of cycles during which motion has not occurred.
  // Will avoid re-publishing zero velocities endlessly.
  int zero_velocity_count_ = 0;

  // Flag for staying inactive while there are no incoming commands
  bool wait_for_servo_commands_ = true;

  // Flag saying if the filters were updated during the timer callback
  bool updated_filters_ = false;

  // Nonzero status flags
  bool have_nonzero_twist_stamped_ = false;
  bool have_nonzero_joint_command_ = false;
  bool have_nonzero_command_ = false;

  // Incoming command messages
  geometry_msgs::msg::TwistStamped twist_stamped_cmd_;
  control_msgs::msg::JointJog joint_servo_cmd_;

  const moveit::core::JointModelGroup* joint_model_group_;

  moveit::core::RobotStatePtr kinematic_state_;

  // incoming_joint_state_ is the incoming message. It may contain passive joints or other joints we don't care about.
  // (mutex protected below)
  // internal_joint_state_ is used in servo calculations. It shouldn't be relied on to be accurate.
  // original_joint_state_ is the same as incoming_joint_state_ except it only contains the joints the servo node acts
  // on.
  sensor_msgs::msg::JointState internal_joint_state_, original_joint_state_;
  std::map<std::string, std::size_t> joint_state_name_map_;

  std::vector<LowPassFilter> position_filters_;

  trajectory_msgs::msg::JointTrajectory::SharedPtr last_sent_command_;

  // ROS
  rclcpp::TimerBase::SharedPtr timer_;
  double period_;  // The loop period, in seconds
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_sub_;
  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr twist_stamped_sub_;
  rclcpp::Subscription<control_msgs::msg::JointJog>::SharedPtr joint_cmd_sub_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr collision_velocity_scale_sub_;
  rclcpp::Publisher<std_msgs::msg::Int8>::SharedPtr status_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr worst_case_stop_time_pub_;
  rclcpp::Publisher<trajectory_msgs::msg::JointTrajectory>::SharedPtr trajectory_outgoing_cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr multiarray_outgoing_cmd_pub_;
  rclcpp::Service<moveit_msgs::srv::ChangeControlDimensions>::SharedPtr control_dimensions_server_;
  rclcpp::Service<moveit_msgs::srv::ChangeDriftDimensions>::SharedPtr drift_dimensions_server_;

  // Status
  StatusCode status_ = StatusCode::NO_WARNING;
  bool stop_requested_ = false;
  bool paused_ = false;
  bool twist_command_is_stale_ = false;
  bool joint_command_is_stale_ = false;
  bool ok_to_publish_ = false;
  double collision_velocity_scale_ = 1.0;

  // Use ArrayXd type to enable more coefficient-wise operations
  Eigen::ArrayXd delta_theta_;
  Eigen::ArrayXd prev_joint_velocity_;

  const int gazebo_redundant_message_count_ = 30;

  uint num_joints_;

  // True -> allow drift in this dimension. In the command frame. [x, y, z, roll, pitch, yaw]
  std::array<bool, 6> drift_dimensions_ = { { false, false, false, false, false, false } };

  // The dimesions to control. In the command frame. [x, y, z, roll, pitch, yaw]
  std::array<bool, 6> control_dimensions_ = { { true, true, true, true, true, true } };

  // Amount we sleep when waiting
  rclcpp::Rate default_sleep_rate_{ 100 };

  // latest_state_mutex_ is used to protect the state below it
  mutable std::mutex latest_state_mutex_;
  sensor_msgs::msg::JointState::ConstSharedPtr incoming_joint_state_;
  Eigen::Isometry3d tf_moveit_to_robot_cmd_frame_;
  geometry_msgs::msg::TwistStamped::ConstSharedPtr latest_twist_stamped_;
  control_msgs::msg::JointJog::ConstSharedPtr latest_joint_cmd_;
  rclcpp::Time latest_twist_command_stamp_ = rclcpp::Time(0.);
  rclcpp::Time latest_joint_command_stamp_ = rclcpp::Time(0.);
  bool latest_nonzero_twist_stamped_ = false;
  bool latest_nonzero_joint_cmd_ = false;
};
}  // namespace moveit_servo