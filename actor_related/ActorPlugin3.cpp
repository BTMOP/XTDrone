/*
 * Copyright (C) 2016 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <functional>

#include <ignition/math.hh>
#include "gazebo/physics/physics.hh"
#include "ActorPlugin3.h"
#include "common.h"

using namespace gazebo;
using namespace std;

GZ_REGISTER_MODEL_PLUGIN(ActorPlugin3)

#define WALKING_ANIMATION "walking3"
int flag3 = 0;
int count_flag3 = 0;
/////////////////////////////////////////////////
ActorPlugin3::~ActorPlugin3()
{
  update_connection_->~Connection();
}

/////////////////////////////////////////////////
void ActorPlugin3::Load(physics::ModelPtr _model, sdf::ElementPtr _sdf)
{
  this->sdf = _sdf;
  this->actor = boost::dynamic_pointer_cast<physics::Actor>(_model);
  this->world = this->actor->GetWorld();
  this->connections.push_back(event::Events::ConnectWorldUpdateBegin(
          std::bind(&ActorPlugin3::OnUpdate, this, std::placeholders::_1)));

  this->Reset();

  if (_sdf->HasElement("robotNamespace"))
    namespace_ = _sdf->GetElement("robotNamespace")->Get<std::string>();
  else
    gzerr << "[ActorPlugin3] Please specify a robotNamespace.\n";
  node_handle_ = transport::NodePtr(new transport::Node());
  node_handle_->Init(namespace_);

  // Listen to the update event. This event is broadcast every
  // simulation iteration.
  update_connection_ = event::Events::ConnectWorldUpdateBegin(boost::bind(&ActorPlugin3::OnUpdate, this, _1));

  //Subscribe the command actor pose
  cmd_pose_sub_3 = node_handle_->Subscribe<mav_msgs::msgs::CmdActor>("cmd_actor_pose3", &ActorPlugin3::CmdPoseCallback, this);

  //Publish the command actor pose
  cmd_pose_pub_3 = node_handle_->Advertise<mav_msgs::msgs::CmdActor>("cmd_actor_pose3", 1);

  // FIXME: Commented out to prevent warnings about queue limit reached.
  actor_pose_pub_3 = node_handle_->Advertise<mav_msgs::msgs::Actor>("actor_pose3", 1);

}

void ActorPlugin3::CmdPoseCallback(CmdActorPtr &cmd_msg)
{
  target[0] = cmd_msg->cmd_actor_pose_x();
  target[1] = cmd_msg->cmd_actor_pose_y();
  target[2] = cmd_msg->cmd_actor_pose_z();
  //target = ignition::math::Vector3d(10, 10, 1.0191);
  //std::cout << "I'm here!" << endl;
}

/////////////////////////////////////////////////
void ActorPlugin3::Reset()
{
  this->lastUpdate = 0;
/*
  if (this->sdf && this->sdf->HasElement("target"))
    this->target = this->sdf->Get<ignition::math::Vector3d>("target");
  else
*/
  this->target = ignition::math::Vector3d(0, 0, 1.0191);

  auto skelAnims = this->actor->SkeletonAnimations();
  if (skelAnims.find(WALKING_ANIMATION) == skelAnims.end())
  {
    gzerr << "Skeleton animation " << WALKING_ANIMATION << " not found.\n";
  }
  else
  {
    // Create custom trajectory
    this->trajectoryInfo.reset(new physics::TrajectoryInfo());
    this->trajectoryInfo->type = WALKING_ANIMATION;
    this->trajectoryInfo->duration = 1.0;

    this->actor->SetCustomTrajectory(this->trajectoryInfo);
  }
}

/////////////////////////////////////////////////
void ActorPlugin3::ChooseNewTarget()
{
  ignition::math::Vector3d newTarget(this->target);
  while ((newTarget - this->target).Length() < 2.0)
  {
    newTarget.X(ignition::math::Rand::DblUniform(-50, 100));
    newTarget.Y(ignition::math::Rand::DblUniform(-50, 50));

    for (unsigned int i = 0; i < this->world->ModelCount(); ++i)
    {
      double dist = (this->world->ModelByIndex(i)->WorldPose().Pos()
          - newTarget).Length();
      if (dist < 2.0)
      {
        newTarget = this->target;
        break;
      }
    }
  }
  this->target = newTarget;
}

/////////////////////////////////////////////////
void ActorPlugin3::OnUpdate(const common::UpdateInfo &_info)
{
  this->velocity = 0.8;
  // Time delta
  double dt = (_info.simTime - this->lastUpdate).Double();

  ignition::math::Pose3d pose = this->actor->WorldPose();
  ignition::math::Vector3d pos = this->target - pose.Pos();
  ignition::math::Vector3d rpy = pose.Rot().Euler();

  double distance = pos.Length();

  // Choose a new target position if the actor has reached its current target.
  if (distance < 0.01)
  {
    // FIXME: Commented out to prevent swerve after actor reached its target.
    //this->ChooseNewTarget();
    pos = this->target - pose.Pos();
  }
  // Choose a suitable velocity at different distance.
  if (distance > 1.0)
  {
    this->velocity = velocity / distance;
  }
  else if (distance <= 1.0)
  {
    this->velocity = velocity;
  }

  // Compute the yaw orientation.
  ignition::math::Angle yaw = atan2(pos.Y(), pos.X()) + 1.5707 - rpy.Z();
  yaw.Normalize();

  // Rotate in place, instead of jumping.
  if (std::abs(yaw.Radian()) > IGN_DTOR(10))
  {
    pose.Rot() = ignition::math::Quaterniond(1.5707, 0, rpy.Z() + yaw.Radian()*0.001);
  }
  else
  {
    pose.Pos() += pos * this->velocity * dt;
    pose.Rot() = ignition::math::Quaterniond(1.5707, 0, rpy.Z()+yaw.Radian());
  }

  if (flag3 == 0)
  {
    pose.Pos().X(-30);
    pose.Pos().Y(30);
    pose.Pos().Z(1.0191);
    cmd_pose_msg.set_cmd_actor_pose_x(-30);
    cmd_pose_msg.set_cmd_actor_pose_y(30);
    count_flag3 = 0;
  }
  else
  {
  // Make sure the actor stays within bounds
    pose.Pos().X(std::max(-50.0, std::min(100.0, pose.Pos().X())));
    pose.Pos().Y(std::max(-50.0, std::min(50.0, pose.Pos().Y())));
    pose.Pos().Z(1.0191);
  }

  // Distance traveled is used to coordinate motion with the walking
  // animation
  double distanceTraveled = (pose.Pos() -
      this->actor->WorldPose().Pos()).Length();
  //std::cout << "this->actor->WorldPose().Pos():" << this->actor->WorldPose().Pos().X() << ";" << this->actor->WorldPose().Pos().Y();
  this->actor->SetWorldPose(pose, false, false);
  this->actor->SetScriptTime(this->actor->ScriptTime() + (distanceTraveled * 5.0));
  this->lastUpdate = _info.simTime;
  
  gazebo::msgs::Vector3d* actor_pose = new gazebo::msgs::Vector3d();
  actor_pose->set_x(pose.Pos().X());
  actor_pose->set_y(pose.Pos().Y());
  actor_pose->set_z(pose.Pos().Z());

  // FIXME: Commented out to prevent warnings about queue limit reached.
  pose_msg.set_allocated_actor_pose(actor_pose);
  actor_pose_pub_3->Publish(pose_msg);


  if ((abs(target[0]-pose.Pos().X())<0.1)&&(abs(target[1]-pose.Pos().Y())<0.1))
  {
    if (count_flag3 == 0)
    {
      flag3 ++;
    }
    count_flag3 = 1;
    if (flag3 ==5)
      flag3 = 1;
  }

  if (flag3 == 1)
  {
    cmd_pose_msg.set_cmd_actor_pose_x(-15);
    cmd_pose_msg.set_cmd_actor_pose_y(15);
    count_flag3 = 0;
  }

  if (flag3 == 2)
  {
    cmd_pose_msg.set_cmd_actor_pose_x(-15);
    cmd_pose_msg.set_cmd_actor_pose_y(45);
    count_flag3 = 0;
  }

  if (flag3 == 3)
  {
    cmd_pose_msg.set_cmd_actor_pose_x(-45);
    cmd_pose_msg.set_cmd_actor_pose_y(45);
    count_flag3 = 0;
  }

  if (flag3 == 4)
  {
    cmd_pose_msg.set_cmd_actor_pose_x(-45);
    cmd_pose_msg.set_cmd_actor_pose_y(15);
    count_flag3 = 0;
  }

  cmd_pose_msg.set_cmd_actor_pose_z(1.0191);
  cmd_pose_pub_3->Publish(cmd_pose_msg);

  //std::cout << "[XTDrone_Actor_Plugin3]: Publish topic actor_pose_pub3"<< std::endl;
  //std::cout << "Target_Position3:  " << target[0] << "," << target[1] << "," << target[2] << endl;
  //std::cout << "Actor_Position3:  " << dec << pose.Pos().X() << "," << pose.Pos().Y() << "," << pose.Pos().Z() << endl;

}
