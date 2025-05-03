#include <vk_engine.h>
#include <vk_types.h>

struct SVGFSettings {
    float radius;
};


class SVGFHandler {
public:
    SVGFHandler(VulkanEngine* engine, VkExtent2D extent);

    ~SVGFHandler();



    void init(VulkanEngine* engine, VkExtent2D extent);

    void Execute(VkCommandBuffer cmd);

private:

    AllocatedImage illuminatuion;
    AllocatedImage prevIllumination;
    AllocatedImage NormalAndDepth;
    AllocatedImage prevNormalAndDepth;
    AllocatedImage historyLength;
    AllocatedImage prevHistoryLength;
    AllocatedImage normal;

    VkPipeline m_reprojectionPipeline;
    VkPipelineLayout m_reprojPipelineLayout;

    VkDescriptorSet m_reprojDscSet;
    VkDescriptorSetLayout m_reprojDscSetLayout;

    //void PackNormalDepth(VkCommandBuffer cmd);

    void create_frame_buffers(VulkanEngine* engine);

    void create_reprojection_pipeline(VulkanEngine* engine);
    void create_descriptors(VulkanEngine* engine);

    /// <summary>
    /// Needed inputs
    /// 
    /// </summary>
    /// <param name="cmd"></param>

    void Reprojection(VkCommandBuffer cmd);

    void FilterMoments(VkCommandBuffer cmd);

    void WaveletFilter(VkCommandBuffer cmd);
};