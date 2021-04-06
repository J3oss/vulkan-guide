set(CMAKE_CXX_STANDARD 17)

add_executable(Asset-Baker Asset-Baker.cpp)

target_include_directories(Asset-Baker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Asset-Baker PUBLIC stb_image json lz4 Asset-Lib glm assimp)
