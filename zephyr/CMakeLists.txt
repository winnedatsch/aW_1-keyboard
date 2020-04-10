cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(nrf52-zephyr-keyboard)

FILE(GLOB app_sources ../src/*.c*)
target_sources(app PRIVATE ${app_sources})
