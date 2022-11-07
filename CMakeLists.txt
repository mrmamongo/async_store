cmake_minimum_required(VERSION 3.22)
project(async_store)

set(CMAKE_CXX_STANDARD 20)

add_executable(async_store main.cpp async_store.h utils/thread_safe.h)
