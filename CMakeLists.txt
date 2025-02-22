# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.24)




project ("vulkan_engine")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/*.rchit"
    "${PROJECT_SOURCE_DIR}/shaders/*.rmiss"
    "${PROJECT_SOURCE_DIR}/shaders/*.rgen"
    "${PROJECT_SOURCE_DIR}/shaders/*.rint"
    "${PROJECT_SOURCE_DIR}/shaders/*.rahit"


    "${PROJECT_SOURCE_DIR}/shaders/**/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.rchit"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.rmiss"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.rgen"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.rint"
    "${PROJECT_SOURCE_DIR}/shaders/**/*.rahit"
    )


# print current directory


foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/spv/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E env "VK_LAYER_DISABLES=VK_VALIDATION_FEATURE_DISABLE_SHADER_VALIDATION_CACHE_EXT"
            ${GLSL_VALIDATOR} -g --target-env vulkan1.3 -V ${GLSL} "${PROJECT_SOURCE_DIR}/myconfig.conf" -o ${SPIRV} 
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
