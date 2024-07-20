#pragma once 
#include <vk_types.h>

namespace vkutil {

    bool load_shader_module(const char* filePath,
        VkDevice device,
        VkShaderModule* outShaderModule);    

    

}


class PipelineBuilder {
    public:
        std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    
        VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
        VkPipelineRasterizationStateCreateInfo _rasterizer;
        VkPipelineColorBlendAttachmentState _colorBlendAttachment;
        VkPipelineMultisampleStateCreateInfo _multisampling;
        VkPipelineLayout _pipelineLayout;
        VkPipelineDepthStencilStateCreateInfo _depthStencil;
        VkPipelineRenderingCreateInfo _renderInfo;
        VkFormat _colorAttachmentformat;

        PipelineBuilder(){ clear(); }

        void clear();

        VkPipeline build_pipeline(VkDevice device);
};