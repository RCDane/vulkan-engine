# Print the build type
message("CMAKE_BUILD_TYPE is: ${CMAKE_BUILD_TYPE}")

# Print the runtime DLLs
message("TARGET_RUNTIME_DLLS: $<TARGET_RUNTIME_DLLS:engine>")

# Add source to this project's executable.
add_executable(engine 
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp
  camera.cpp
  camera.h
  shadows.h
  shadows.cpp
  raytracing.h
  raytracing.cpp
  vk_extensions.h
  vk_extensions.cpp
  alignment.hpp
  vk_buffers.cpp
  vk_buffers.h
  svgf.h
  svgf.cpp
  )

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image  vkbootstrap imgui)


target_link_libraries(engine PRIVATE SDL3::SDL3)
target_link_libraries(engine
  PUBLIC
    fastgltf::fastgltf
    simdjson::simdjson   # note the correct imported target name
)

target_include_directories(engine PUBLIC
  $<BUILD_INTERFACE:${fastgltf_SOURCE_DIR}/include>
)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp> <glm/vec4.hpp> <vulkan/vulkan.h>)
if(WIN32)
  add_custom_command(
      TARGET engine POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:engine>
      VERBATIM
  )
endif()
# # Debugging the add_custom_command
# if (WIN32)
#     add_custom_command(TARGET dengine POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E echo "Post-build step for engine"
#     COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}
#     COMMAND ${CMAKE_COMMAND} -E echo "Directory created at $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}"
#     COMMAND ${CMAKE_COMMAND} -E echo "Runtime DLLs: $<TARGET_RUNTIME_DLLS:engine>"
#     COMMAND ${CMAKE_COMMAND} -E echo "Copying runtime DLLs to $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}/"
#     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}/
#     )
set_target_properties(engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/bin/Release
  )
set_property(TARGET engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")