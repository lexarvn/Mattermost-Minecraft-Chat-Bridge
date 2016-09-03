cmake_minimum_required (VERSION 2.6)
project (Mattermost-Minecraft-Chat-Bridge)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3 -Wall -lboost_filesystem -lboost_system -lcurl")

find_package(Threads REQUIRED)
find_package(Boost 1.54.0 COMPONENTS regex system thread coroutine context filesystem date_time REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(Mattermost-Minecraft-Chat-Bridge Mattermost-Minecraft-Chat-Bridge.cpp)
target_link_libraries(Mattermost-Minecraft-Chat-Bridge ${Boost_LIBRARIES})
target_link_libraries(Mattermost-Minecraft-Chat-Bridge ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(Mattermost-Minecraft-Chat-Bridge curl)
