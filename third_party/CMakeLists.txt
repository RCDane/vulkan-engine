find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)

add_library(stb_image INTERFACE)

# add_subdirectory(fastgltf)
add_subdirectory(fmt EXCLUDE_FROM_ALL)
# add_subdirectory(SDL EXCLUDE_FROM_ALL)

target_sources(vkbootstrap PRIVATE 
  vkbootstrap/VkBootstrap.h
  vkbootstrap/VkBootstrap.cpp
  )

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)

target_include_directories(glm INTERFACE glm)
# set(CPM_FETCHCONTENT ON CACHE BOOL "" FORCE)
set(CPM_USE_LOCAL_PACKAGES ON CACHE BOOL "")
set(CPM_LOCAL_PACKAGES_ONLY ON CACHE BOOL "")


option(ENABLE_CPM "Enable CPM" ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

if(ENABLE_CPM)
  # include(CPM)
  CPMAddPackage("gh:libsdl-org/SDL#release-2.30.4")
  CPMAddPackage("gh:wolfpld/tracy#v0.11.0")
  CPMAddPackage("gh:simdjson/simdjson#v3.12.3")
  CPMAddPackage(
    NAME fastgltf
    GITHUB_REPOSITORY "spnda/fastgltf"
    VERSION 0.8.0
    OPTIONS
      "FASTGLTF_ENABLE_TESTS OFF"
      "FASTGLTF_ENABLE_EXAMPLES OFF"
  )



  option ( TRACY_ENABLE " " OFF )
  option ( TRACY_ON_DEMAND " " OFF )
endif()

add_library(sdl2 INTERFACE)

target_include_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Include/SDL2 )
target_link_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Lib )

target_link_libraries(sdl2 INTERFACE SDL2 SDL2main Tracy::TracyClient)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE 
  imgui/imgui.h
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_impl_vulkan.cpp
  imgui/imgui_impl_sdl2.cpp
  )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

target_include_directories(stb_image INTERFACE stb_image)
