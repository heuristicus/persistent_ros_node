#include <ros/ros.h>
#include <ros/time.h>
#include <condition_variable>
#include <stdio.h>
#include <signal.h>
#include <std_msgs/String.h>

std::atomic_bool running(true);
std::atomic_bool restart(false);
std::condition_variable timeout_cv;

void Shutdown()
{
    ROS_INFO("Calling ros::shutdown()");
    try
    {
        running = false;
        ros::shutdown();
        timeout_cv.notify_one();
    }
    catch (const std::system_error &e)
    {
        std::cout << "In Shutdown. Caught system_error with code " << e.code()
                  << " meaning " << e.what() << '\n';
    }
}

void mySignalHandler(int sig)
{
    Shutdown();
}

void receiveString(const std_msgs::String& msg) {
    std::cout << "received string " << msg.data.c_str() << std::endl;
    ROS_INFO("Received string %s", msg.data.c_str());
}

int main(int argc, char **argv)
{
    while (running.load())
    {
        restart = false;
        std::cout << "Persistent node starting" << std::endl;

        // Do the ROS Stuff
        ros::init(
            argc, argv, "persistent_node",
            ros::init_options::NoSigintHandler | ros::init_options::AnonymousName);
        signal(SIGINT, mySignalHandler);
        signal(SIGTERM, mySignalHandler);

        std::chrono::microseconds loop_period(500 * 1000);
        std::mutex mutex;
        std::unique_lock<std::mutex> lk(mutex);

        while (running.load() && !restart.load())
        {
            restart = false;
            const int inactive_log_every_n(120);
            const int active_log_every_n(120);
            int loop_count(inactive_log_every_n);

            while (!ros::master::check() && running.load() && !restart.load())
            {
                // while we have no master available
                if (loop_count++ == inactive_log_every_n)
                {
                    loop_count = 0;
                    std::cout << "Waiting for Master to become available." << std::endl;
                }
                timeout_cv.wait_for(lk, loop_period, [] { return !running.load() || restart.load(); });
            }
            if (!running.load() || restart.load())
            {
                break;
            }
            std::cout << "Master is available. Starting Node." << std::endl;
            // master is now available, make a node handle
            ros::NodeHandle nh;
            ros::AsyncSpinner spinner(1); // Use 8 threads
            spinner.start();

            ros::Subscriber stringSub = nh.subscribe("string_in", 1, receiveString);
            ros::Publisher stringPub = nh.advertise<std_msgs::String>("string_out", 1);
            std_msgs::String outMsg;
            outMsg.data = "output";

            std::cout << "Node started." << std::endl;
            ros::Rate loop_rate(1);
            // if we lose comms with the master we exit this block and the node handle will go out of scope
            while (ros::ok() && ros::master::check() && !restart.load())
            {
                std::cout << "sleeping " << std::endl;
                ROS_INFO("Sleeping...");
                stringPub.publish(outMsg);
                loop_rate.sleep();
            }
            std::cout << "Node Stopped." << std::endl;
            spinner.stop();
        } // end while running

    } // end outer while running

    std::cout << "Persistent node done." << std::endl;

    return 0;
}
