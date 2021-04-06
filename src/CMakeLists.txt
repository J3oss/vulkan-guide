
# Add source to this project's executable.
add_executable(vulkan_guide main.cpp
                            vk_engine.h
                            vk_engine.cpp
                            vk_types.h
                            vk_initializers.h
                            vk_initializers.cpp
                            vk_pipeline.h
                            vk_pipeline.cpp
                            vk_mesh.h
                            vk_mesh.cpp
                            vk_textures.h
                            vk_textures.cpp
                            camera.h
                            camera.cpp)

set_property(TARGET vulkan_guide PROPERTY CXX_STANDARD 17)
set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" )

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2 assimp vkbootstrap vma glm tinyobjloader imgui stb_image Asset-Lib)

add_dependencies(vulkan_guide Shaders)
