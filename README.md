# vulkan-engine
This project is for a special course in Computer Graphics, focused on implementing Raytracing using C++ and Vulkan.

Features:
- glTF 2.0 scene loading with FastGLTF.
- Deferred rasterization with G-buffer outputs for albedo, normals, metallic/roughness, emissive data, and depth.
- glTF metallic-roughness PBR materials with base-color, normal, metallic-roughness, and emissive textures.
- Cook-Torrance shading using GGX, Smith visibility, and Schlick Fresnel.
- Hardware-accelerated Vulkan ray tracing with BLAS/TLAS acceleration structures and a shader binding table.
- Ray-traced direct lighting, environment-map misses, and visibility-tested shadows.
- Stochastic indirect-light sampling with cosine-hemisphere and GGX VNDF strategies.
- Optional progressive accumulation, reset when the camera moves.
- SVGF-inspired denoising: temporal reprojection/history validation, luminance moments, edge-aware filtering, and multi-scale à-trous wavelet passes.
- Albedo remodulation after denoising, plus timestamp profiling for the filter stages.
- Frustum culling, directional lighting, directional shadow mapping, environment rendering, and tone mapping.



Resources and Inspiration:
- The Vulkan Guide: https://vkguide.dev/
  -  The codebase is built on top of the vulkan guide as it's base.
- The Vulkan Tutorial: https://vulkan-tutorial.com/
- NVIDIA Vulkan Raytracing tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
  -  The raytracing implementation follows the one in this tutorial. Although with changes to match my Vulkan abstractions and interfaces.
- Sascha Willems Vulkan Examples and Demos: https://github.com/SaschaWillems/Vulkan
  - Great source for examples and inspiration. 
