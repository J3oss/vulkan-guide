find_package(Vulkan REQUIRED)

add_library(imgui STATIC)
add_library(vma INTERFACE)
add_library(glm INTERFACE)
add_library(sdl2 INTERFACE)
add_library(vkbootstrap STATIC)
add_library(stb_image INTERFACE)
add_library(tinyobjloader STATIC)

add_library(lz4 STATIC)
add_library(json INTERFACE)

add_library(spirv_reflect STATIC)

#add_library(assimp SHARED IMPORTED)
#set_target_properties( assimp PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/assimp/lib)
#vkbootstrap
target_sources(vkbootstrap PRIVATE
    vkbootstrap/VkBootstrap.h
    vkbootstrap/VkBootstrap.cpp
    )
target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)

#vma
target_include_directories(vma INTERFACE vma)

#glm
target_include_directories(glm INTERFACE glm)

#objloader
target_sources(tinyobjloader PRIVATE
    tinyobjloader/tiny_obj_loader.h
    tinyobjloader/tiny_obj_loader.cc
    )
target_include_directories(tinyobjloader PUBLIC tinyobjloader)

#sdl
target_include_directories(sdl2 INTERFACE sdl2)
target_link_libraries(sdl2 INTERFACE  SDL2)

#imgui
target_sources(imgui PRIVATE
    imgui/imgui.h
    imgui/imgui.cpp

    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp

    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_sdl.cpp
    )
target_include_directories(imgui PUBLIC imgui)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)

#stb_image
target_include_directories(stb_image INTERFACE stb_image)

#json
target_include_directories(json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/json" )

#lz4
target_include_directories(lz4 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lz4" )
target_sources(lz4 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4.h"
                           "${CMAKE_CURRENT_SOURCE_DIR}/lz4/lz4.c")

#spirv reflect
target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect" )
target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/include" )
target_sources(spirv_reflect PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.c"
)
