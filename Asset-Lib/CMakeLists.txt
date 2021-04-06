set(CMAKE_CXX_STANDARD 17)

add_library (Asset-Lib STATIC asset_loader.h
                       asset_loader.cpp
                       texture_asset.h
                       texture_asset.cpp
                       mesh_asset.h
                       mesh_asset.cpp)

target_include_directories(Asset-Lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(Asset-Lib PRIVATE json lz4)
