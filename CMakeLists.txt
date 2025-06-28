cmake_minimum_required (VERSION 3.16)
cmake_policy(SET CMP0167 NEW)
project ("image-processor")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system json)
find_package(OpenCV REQUIRED)


add_subdirectory(Server)
add_subdirectory(Client)