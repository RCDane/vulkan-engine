#pragma once
#ifndef SVGF_H
#include <vk_engine.h>
#include <vk_types.h>

struct SVGFSettings {
    float radius;
};


class SVGFHandler {
public:
    SVGFHandler(VulkanEngine* engine, VkExtent2D extent);
    SVGFHandler();

    ~SVGFHandler();



    void init(VulkanEngine* engine, VkExtent2D extent);

    void Execute(VkCommandBuffer cmd, VulkanEngine* engine);

private:

    AllocatedImage illumination;
    AllocatedImage prevIllumination;
    AllocatedImage normalFWidthZWidth;
    AllocatedImage prevNormalFWidthZWidth;
    AllocatedImage historyLength;
    AllocatedImage prevHistoryLength;
    AllocatedImage prevMoments;
	AllocatedImage packedDepthNormal;

    VkPipeline m_packNormalDepthPipeline;
    VkPipelineLayout m_NormalDepthPipelineLayout;

    VkPipeline m_reprojectionPipeline;
    VkPipelineLayout m_reprojPipelineLayout;

    VkPipeline m_momentsFilterPipeline;
    VkPipelineLayout m_momentsFilterPipelineLayout;

    VkPipeline m_atrousPipeline;
    VkPipelineLayout m_atrousPipelineLayout;

    VkPipeline m_modulatePipeline;
    VkPipelineLayout m_modulatePipelineLayout;

    VkDescriptorSet m_reprojDscSet;
    VkDescriptorSetLayout m_reprojDscSetLayout;

    VkDescriptorSet m_momentsFilterDscSet;
    VkDescriptorSetLayout m_momentsFilterDscSetLayout;

    VkDescriptorSet m_atrousDscSet;
    VkDescriptorSetLayout m_atrousDscSetLayout;

    VkDescriptorSet m_modulateDscSet;
    VkDescriptorSetLayout m_modulateDscSetLayout;


    VkDescriptorSet m_packNormalDepthDscSet;
    VkDescriptorSetLayout m_packNormalDepthDscSetLayout;

    void PackNormalDepth(VkCommandBuffer cmd, VulkanEngine* engine);

    void create_frame_buffers(VulkanEngine* engine);

    void create_reprojection_pipeline(VulkanEngine* engine);
	void create_moments_filter_pipeline(VulkanEngine* engine);
	void create_wavelet_filter_pipeline(VulkanEngine* engine);
    void create_modulate_pipeline(VulkanEngine* engine);
    void create_packing_pipeline(VulkanEngine* engine);


    void create_descriptors(VulkanEngine* engine);

    /// <summary>
    /// Needed inputs
    /// 
    /// </summary>
    /// <param name="cmd"></param>

    void Reprojection(VkCommandBuffer cmd, VulkanEngine* engine);

    void FilterMoments(VkCommandBuffer cmd, VulkanEngine* engine);

    void WaveletFilter(VkCommandBuffer cmd, VulkanEngine* engine);

    void Modulate(VkCommandBuffer cmd, VulkanEngine* engine);

};

#endif // !SVGF_H