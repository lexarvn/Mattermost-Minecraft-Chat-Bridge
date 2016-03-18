cmake_minimum_required (VERSION 2.6)
project (Mattermost-Minecraft-Chat-Bridge)

# The version number.
set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

# Flags
add_definitions(-std=gnu++11)

# add the executable
add_executable(Mattermost-Minecraft-Chat-Bridge Mattermost-Minecraft-Chat-Bridge.cpp)
