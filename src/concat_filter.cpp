/*
 * Copyright 2019 Shota Hirama
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
 */

#include <velodyne_concat_filter/concat_filter.h>

namespace velodyne_concat_filter
{
ConcatFilter::ConcatFilter() : tf_listener_(tf_buffer_) {}

void ConcatFilter::onInit()
{
  nh_ = getNodeHandle();
  concat_point_pub_ = nh_.advertise<sensor_msgs::PointCloud2>("concat_points", 1);
  std::vector<std::string> topics{"/velodyne_front/velodyne_points", "/velodyne_rear/velodyne_points", "/velodyne_right/velodyne_points", "/velodyne_left/velodyne_points", "/velodyne_top/velodyne_points"};
  for (size_t i = 0; i < topics.size(); i++) {
    auto sub = std::make_shared<message_filters::Subscriber<sensor_msgs::PointCloud2>>(nh_, topics[i], 1);
    sub_.emplace_back(sub);
  }
  sync_ = std::make_shared<message_filters::Synchronizer<SyncPolicyT>>(SyncPolicyT(10), *sub_[0], *sub_[1], *sub_[2], *sub_[3], *sub_[4]);
  sync_->registerCallback(boost::bind(&ConcatFilter::callback, this, _1, _2, _3, _4, _5));
}

void ConcatFilter::callback(const sensor_msgs::PointCloud2ConstPtr &msg1, const sensor_msgs::PointCloud2ConstPtr &msg2, const sensor_msgs::PointCloud2ConstPtr &msg3, const sensor_msgs::PointCloud2ConstPtr &msg4, const sensor_msgs::PointCloud2ConstPtr &msg5)
{
  std::vector<sensor_msgs::PointCloud2ConstPtr> msgs{msg1, msg2, msg3, msg4, msg5};
  std::vector<PointCloudT::Ptr> clouds(msgs.size());
  PointCloudT::Ptr concat_cloud = boost::make_shared<PointCloudT>();
  try {
    for (size_t i = 0; i < msgs.size(); i++) {
      const geometry_msgs::TransformStamped transformStamped = tf_buffer_.lookupTransform("base_link", msgs[i]->header.frame_id, ros::Time(0), ros::Duration(0.1));
      sensor_msgs::PointCloud2 transform_cloud;
      tf2::doTransform(*msgs[i], transform_cloud, transformStamped);
      clouds[i] = boost::make_shared<PointCloudT>();
      pcl::fromROSMsg(transform_cloud, *clouds[i]);
    }
  } catch (tf2::TransformException &ex) {
    ROS_ERROR("%s", ex.what());
    return;
  }
  for (size_t i = 0; i < clouds.size(); i++) {
    *concat_cloud += *clouds[i];
  }
  sensor_msgs::PointCloud2 pubmsg;
  pcl::toROSMsg(*concat_cloud, pubmsg);
  pubmsg.header.stamp = ros::Time::now();
  pubmsg.header.frame_id = "base_link";
  concat_point_pub_.publish(pubmsg);
}
}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(velodyne_concat_filter::ConcatFilter, nodelet::Nodelet);