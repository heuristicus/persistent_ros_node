cmake_minimum_required(VERSION 3.0.2)
project(persistent_node)

add_compile_options(-std=c++11)

find_package(catkin REQUIRED COMPONENTS
  roscpp
)

catkin_package()

include_directories(
#  include
  ${catkin_INCLUDE_DIRS}
)

add_executable(persistent_node src/persistent_node.cpp)
target_link_libraries(persistent_node
   ${catkin_LIBRARIES}
)