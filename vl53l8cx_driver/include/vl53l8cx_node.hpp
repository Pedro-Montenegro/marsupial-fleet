#ifndef VL53L8CX_NODE_HPP_
#define VL53L8CX_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32_multi_array.hpp>

extern "C" {
#include "vl53l8cx_api.h"
#include "platform.h"
}

#include <vector>

#define SENSOR_COUNT 2
#define GRID 64

class VL53L8CXNode : public rclcpp::Node
{
public:

    VL53L8CXNode();

private:

    VL53L8CX_Configuration dev[SENSOR_COUNT];
    VL53L8CX_ResultsData results[SENSOR_COUNT];

    rclcpp::Publisher<std_msgs::msg::Int32MultiArray>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;

    void init_sensors();
    void update();
};

#endif