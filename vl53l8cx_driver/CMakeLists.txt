cmake_minimum_required(VERSION 3.8)
project(vl53l8cx_driver)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(vl53l8cx_node

  src/vl53l8cx_node.cpp

  VL53L8CX_ULD_API/src/vl53l8cx_api.c
  VL53L8CX_ULD_API/src/platform.c
)

target_include_directories(vl53l8cx_node PUBLIC

  ${CMAKE_CURRENT_SOURCE_DIR}/VL53L8CX_ULD_API/inc
)

target_link_libraries(vl53l8cx_node
  gpiod
)

ament_target_dependencies(
  vl53l8cx_node
  rclcpp
  sensor_msgs
)

install(TARGETS
  vl53l8cx_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()