/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2020, PickNik LLC
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
 *   * Neither the name of PickNik LLC nor the names of its
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

/*      Title     : servo_server.h
 *      Project   : moveit_servo
 *      Created   : 07/13/2020
 *      Author    : Adam Pettinger
 */

// TODO(adamp): license correct?

#pragma once

#include <moveit_servo/servo.h>

namespace moveit_servo
{
class ServoServer : public rclcpp::Node
{
public:
  ServoServer(const rclcpp::NodeOptions& options);

private:
  void initializeServo();

  rclcpp::TimerBase::SharedPtr initialization_timer_;

  std::unique_ptr<moveit_servo::Servo> servo_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<planning_scene_monitor::PlanningSceneMonitor> planning_scene_monitor_;

  /** \brief Start the servo */
  void startCB(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
          std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_servo_service_; //TODO(adamp): change these from triggers to empty srv?

  /** \brief Stop the servo */
  void stopCB(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
          std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr stop_servo_service_;

  /** \brief Pause servo */
  void pauseCB(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
          std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr pause_servo_service_;

  /** \brief Unpause servo */
  void unpauseCB(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
          std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr unpause_servo_service_;

  // TODO(adamp): Do we actually want to expose these? Might need to define a service then... Could also just leave it for C++ API
  void getCommandFrameTransform();
  void getLatestJointState();
};
} //namespace moveit_servo