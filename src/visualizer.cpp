#include <interactive_markers/menu_handler.h>
#include <pose_graph_tools/PoseGraphEdge.h>
#include <pose_graph_tools/PoseGraphNode.h>
#include <pose_graph_tools/visualizer.h>
#include <visualization_msgs/Marker.h>

using Color = std::tuple<float, float, float>;
// Create a vector of colors (R, G, B), with each component in the range [0, 1]
const std::vector<Color> kColormap = {
    {1.0f, 0.0f, 0.0f},    // Red
    {0.0f, 0.0f, 1.0f},    // Blue
    {1.0f, 1.0f, 0.0f},    // Yellow
    {0.0f, 1.0f, 1.0f},    // Cyan
    {1.0f, 0.0f, 1.0f},    // Magenta
    {0.75f, 0.75f, 0.75f}, // Silver
    {0.5f, 0.0f, 0.0f},    // Maroon
    {0.5f, 0.5f, 0.0f},    // Olive
    {0.0f, 0.5f, 0.0f},    // Dark Green
    {0.0f, 0.0f, 0.5f}     // Dark Blue
};

Visualizer::Visualizer(const ros::NodeHandle &nh) {
  ROS_INFO("Initializing pose graph visualizer");

  // start subscribers
  ros::NodeHandle nl(nh);
  ros::NodeHandle nl_private("~");

  nl_private.param<float>("node_size", node_size_, 0.05);

  pose_graph_sub_ = nl.subscribe<pose_graph_tools::PoseGraph>(
      "graph", 10, &Visualizer::PoseGraphCallback, this);

  // start publishers
  odometry_edge_pub_ =
      nl.advertise<visualization_msgs::Marker>("odometry_edges", 10, false);
  loop_edge_pub_ =
      nl.advertise<visualization_msgs::Marker>("loop_edges", 10, false);
  rejected_loop_edge_pub_ = nl.advertise<visualization_msgs::Marker>(
      "rejected_loop_edges", 10, false);

  graph_node_pub_ =
      nl.advertise<visualization_msgs::Marker>("graph_nodes", 10, false);
  graph_node_id_pub_ =
      nl.advertise<visualization_msgs::Marker>("graph_nodes_ids", 10, false);

  interactive_mrkr_srvr_ =
      std::make_shared<interactive_markers::InteractiveMarkerServer>(
          "interactive_node", "", false);

  ros::spin();
}

void Visualizer::PoseGraphCallback(
    const pose_graph_tools::PoseGraph::ConstPtr &msg) {
  // iterate through nodes in pose graph
  for (const pose_graph_tools::PoseGraphNode &msg_node : msg->nodes) {
    tf::Pose pose;
    tf::poseMsgToTF(msg_node.pose, pose);

    // Fill pose nodes (representing the robot position)
    keyed_poses_[msg_node.robot_id][msg_node.key] = pose;
  }

  // update frame id
  frame_id_ = msg->header.frame_id;

  // iterate through edges in pose graph
  for (const pose_graph_tools::PoseGraphEdge &msg_edge : msg->edges) {
    Node from = std::make_pair(msg_edge.robot_from, msg_edge.key_from);
    Node to = std::make_pair(msg_edge.robot_to, msg_edge.key_to);
    if (msg_edge.type == pose_graph_tools::PoseGraphEdge::ODOM) {
      // initialize first seen robot id
      odometry_edges_.emplace_back(std::make_pair(from, to));
    } else if (msg_edge.type == pose_graph_tools::PoseGraphEdge::LOOPCLOSE) {
      loop_edges_.emplace_back(std::make_pair(from, to));
    } else if (msg_edge.type ==
               pose_graph_tools::PoseGraphEdge::REJECTED_LOOPCLOSE) {
      rejected_loop_edges_.emplace_back(std::make_pair(from, to));
    }
  }

  visualize();
}

geometry_msgs::Point
Visualizer::getPositionFromKey(int robot_id, long unsigned int key) const {
  tf::Vector3 v;
  try {
    v = keyed_poses_.at(robot_id).at(key).getOrigin();
  } catch (const std::out_of_range &e) {
    ROS_ERROR("Robot %d, key %lu not found in keyed_poses_", robot_id, key);
    throw;
  }

  geometry_msgs::Point p;
  p.x = v.x();
  p.y = v.y();
  p.z = v.z();
  return p;
}

// Interactive Marker Menu to click and see key of node
void Visualizer::MakeMenuMarker(const tf::Pose &position,
                                const std::string &id_number) {
  interactive_markers::MenuHandler menu_handler;

  visualization_msgs::InteractiveMarker int_marker;
  int_marker.header.frame_id = frame_id_;
  int_marker.scale = 1.0;
  tf::poseTFToMsg(position, int_marker.pose);
  int_marker.name = id_number;

  visualization_msgs::Marker marker;
  marker.type = visualization_msgs::Marker::SPHERE;
  marker.scale.x = 0.3;
  marker.scale.y = 0.3;
  marker.scale.z = 0.3;
  marker.color.r = 0.0;
  marker.color.g = 1.0;
  marker.color.b = 1.0;
  marker.color.a = 0.5;
  marker.pose.orientation.w = 1.0;

  visualization_msgs::InteractiveMarkerControl control;
  control.interaction_mode = visualization_msgs::InteractiveMarkerControl::MENU;
  control.name = id_number;
  control.markers.push_back(marker);
  control.always_visible = true;
  int_marker.controls.push_back(control);

  menu_handler.insert(id_number);
  interactive_mrkr_srvr_->insert(int_marker);
  menu_handler.apply(*interactive_mrkr_srvr_, int_marker.name);
}

void Visualizer::visualize() {
  // Publish odometry edges.
  if (odometry_edge_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = frame_id_;
    m.ns = frame_id_;
    m.id = 0;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::LINE_LIST;
    for (size_t ii = 0; ii < odometry_edges_.size(); ++ii) {
      int robot_id = odometry_edges_[ii].first.first;
      const auto key1 = odometry_edges_[ii].first.second;
      const auto key2 = odometry_edges_[ii].second.second;

      // TODO(Yun) currently the below color formula
      // means that only support up to 5 robots
      std_msgs::ColorRGBA color;
      color.r = std::get<0>(kColormap[robot_id]);
      color.g = std::get<1>(kColormap[robot_id]);
      color.b = std::get<2>(kColormap[robot_id]);
      color.a = 0.8;
      m.scale.x = node_size_ / 2.f;
      m.pose.orientation.w = 1.0;

      m.points.push_back(getPositionFromKey(robot_id, key1));
      m.points.push_back(getPositionFromKey(robot_id, key2));
      m.colors.push_back(color);
      m.colors.push_back(color);
    }
    odometry_edge_pub_.publish(m);
  }

  // Publish loop closure edges.
  if (loop_edge_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = frame_id_;
    m.ns = frame_id_;
    m.id = 1;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::LINE_LIST;
    m.color.r = 0.0;
    m.color.g = 0.2;
    m.color.b = 1.0;
    m.color.a = 0.8;
    m.scale.x = node_size_ / 2.f;
    m.pose.orientation.w = 1.0;

    for (size_t ii = 0; ii < loop_edges_.size(); ++ii) {
      const auto robot1 = loop_edges_[ii].first.first;
      const auto robot2 = loop_edges_[ii].second.first;
      const auto key1 = loop_edges_[ii].first.second;
      const auto key2 = loop_edges_[ii].second.second;

      m.points.push_back(getPositionFromKey(robot1, key1));
      m.points.push_back(getPositionFromKey(robot2, key2));
    }
    loop_edge_pub_.publish(m);
  }

  // Publish the rejected loop closure edges
  if (rejected_loop_edge_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = frame_id_;
    m.ns = frame_id_;
    m.id = 1;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::LINE_LIST;
    m.color.r = 0.5;
    m.color.g = 0.5;
    m.color.b = 0.5;
    m.color.a = 0.7;
    m.scale.x = node_size_ / 2.f;
    m.pose.orientation.w = 1.0;

    for (size_t ii = 0; ii < rejected_loop_edges_.size(); ++ii) {
      const auto robot1 = rejected_loop_edges_[ii].first.first;
      const auto robot2 = rejected_loop_edges_[ii].second.first;
      const auto key1 = rejected_loop_edges_[ii].first.second;
      const auto key2 = rejected_loop_edges_[ii].second.second;

      m.points.push_back(getPositionFromKey(robot1, key1));
      m.points.push_back(getPositionFromKey(robot2, key2));
    }
    rejected_loop_edge_pub_.publish(m);
  }

  // Publish node IDs in the pose graph.
  if (graph_node_id_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = frame_id_;
    m.ns = frame_id_;

    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
    m.color.r = 1.0;
    m.color.g = 1.0;
    m.color.b = 0.2;
    m.color.a = 0.8;
    m.scale.z = node_size_ / 2.f;
    m.pose.orientation.w = 1.0;

    int id_base = 100;
    int counter = 0;
    for (const auto &robot : keyed_poses_) {
      for (const auto &keyedPose : robot.second) {
        tf::poseTFToMsg(keyedPose.second, m.pose);
        // Display text for the node
        std::string robot_id = std::to_string(keyedPose.first);
        MakeMenuMarker(keyedPose.second, robot_id);
        m.text = robot_id;
        m.id = id_base + keyedPose.first;
        graph_node_id_pub_.publish(m);
      }
    }

    if (interactive_mrkr_srvr_ != nullptr) {
      interactive_mrkr_srvr_->applyChanges();
    }
  }

  // Publish keyframe nodes in the pose graph.
  if (graph_node_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = frame_id_;
    m.ns = frame_id_;
    m.id = 4;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::SPHERE_LIST;
    m.color.r = 0.0;
    m.color.g = 1.0;
    m.color.b = 0.3;
    m.color.a = 0.8;
    m.scale.x = node_size_;
    m.scale.y = node_size_;
    m.scale.z = node_size_;
    m.pose.orientation.w = 1.0;

    for (const auto &robot : keyed_poses_) {
      for (const auto &keyedPose : robot.second) {
        m.points.push_back(getPositionFromKey(robot.first, keyedPose.first));
      }
    }
    graph_node_pub_.publish(m);
  }
}
