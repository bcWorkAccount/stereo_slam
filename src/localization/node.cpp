#include <signal.h>
#include <ros/ros.h>
#include <ros/xmlrpc_manager.h>
#include "localization/base.h"
#include "stereo_slam/SetPointCloud.h"

// Signal-safe flag for whether shutdown is requested
sig_atomic_t volatile g_request_shutdown = 0;

// Replacement SIGINT handler
void sigIntHandler(int sig)
{
  g_request_shutdown = 1;
}

// Replacement "shutdown" XMLRPC callback
void shutdownCallback(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
{
  int num_params = 0;
  if (params.getType() == XmlRpc::XmlRpcValue::TypeArray)
    num_params = params.size();
  if (num_params > 1)
  {
    std::string reason = params[1];
    ROS_WARN("Shutdown request received. Reason: [%s]", reason.c_str());
    g_request_shutdown = 1; // Set flag
  }

  result = ros::xmlrpc::responseInt(1, "", 0);
}

int main(int argc, char **argv)
{
  // Override SIGINT handler
  ros::init(argc, argv, "stereo_slam", ros::init_options::NoSigintHandler);
  signal(SIGINT, sigIntHandler);

  // Override XMLRPC shutdown
  ros::XMLRPCManager::instance()->unbind("shutdown");
  ros::XMLRPCManager::instance()->bind("shutdown", shutdownCallback);

  // Stereo slam class
  ros::NodeHandle nh;
  ros::NodeHandle nh_private("~");
  slam::SlamBase slam_node(nh,nh_private);

  // FIXME: Advertising services here because not working on init();
  ros::ServiceServer start_reconstruction_srv = nh_private.advertiseService("start_reconstruction",
                                                &slam::SlamBase::startReconstruction,
                                                &slam_node);
  ros::ServiceServer stop_reconstruction_srv  = nh_private.advertiseService("stop_reconstruction",
                                                &slam::SlamBase::stopReconstruction,
                                                &slam_node);

  // Do our own spin loop
  while (!g_request_shutdown)
  {
    // Do non-callback stuff
    ros::spinOnce();
    usleep(100000);
  }

  // Finalize stereo slam
  slam_node.finalize();

  // Exit
  ros::shutdown();
}