# This readme is outdated. This branch is being used for the student competition for the high performance graphics conference.

Files related to SVGF can be found in:
shaders/raytracing
src/svgf.h
src/svgf.cpp



# vulkan-engine
This project is for a special course in Computer Graphics, focused on implementing Raytracing using C++ and Vulkan.

Features:
- Simple Blinn-Phong shading.
- GLTF model loading using FastGLTF.
- Frustum Culling.
- Directional Light.
- Directional Shadow Mapping.
- Hardware Accelerated Raytracing.
- Raytraced Shadows.

Planned Features
- Raytraced Ambient Occlusion.
- Raytraced Reflections.


Sponza scene with DamagedHelmet. Showcasing Directional lighting and shadows. 
![image](https://github.com/user-attachments/assets/b0b2e1eb-cbb5-40ef-bd41-79c862b1f56c)





Resources and Inspiration:
- The Vulkan Guide: https://vkguide.dev/
  -  The codebase is built on top of the vulkan guide as it's base.
- The Vulkan Tutorial: https://vulkan-tutorial.com/
- NVIDIA Vulkan Raytracing tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
  -  The raytracing implementation follows the one in this tutorial. Although with changes to match my Vulkan abstractions and interfaces.
- Sascha Willems Vulkan Examples and Demos: https://github.com/SaschaWillems/Vulkan
  - Great source for examples and inspiration. 
