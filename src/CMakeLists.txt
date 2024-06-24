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
)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp> <glm/vec4.hpp> <vulkan/vulkan.h>)

# # Debugging the add_custom_command
# add_custom_command(TARGET engine POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E echo "Post-build step for engine"
#   COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}
#   COMMAND ${CMAKE_COMMAND} -E echo "Directory created at $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}"
#   COMMAND ${CMAKE_COMMAND} -E echo "Runtime DLLs: $<TARGET_RUNTIME_DLLS:engine>"
#   COMMAND ${CMAKE_COMMAND} -E echo "Copying runtime DLLs to $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}/"
#   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>/${CMAKE_BUILD_TYPE}/
# )