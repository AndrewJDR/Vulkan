/*
* Vulkan Example - Using negative viewport heights for changing Vulkan's coordinate system
*
* Note: Requires a device that supports VK_KHR_MAINTENANCE1
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase {
public:
    bool negativeViewport = true;
    int32_t offsety = 0;
    int32_t offsetx = 0;
    int32_t windingOrder = 1;
    int32_t cullMode = (int32_t)VK_CULL_MODE_BACK_BIT;
    int32_t quadType = 0;

    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline = VK_NULL_HANDLE;
    vk::DescriptorSetLayout descriptorSetLayout;
    struct DescriptorSets {
        vk::DescriptorSet CW;
        vk::DescriptorSet CCW;
    } descriptorSets;

    struct Textures {
        vkx::texture::Texture2D CW;
        vkx::texture::Texture2D CCW;
    } textures;

    struct Quad {
        vks::Buffer verticesYUp;
        vks::Buffer verticesYDown;
        vks::Buffer indicesCCW;
        vks::Buffer indicesCW;
        void destroy() {
            verticesYUp.destroy();
            verticesYDown.destroy();
            indicesCCW.destroy();
            indicesCW.destroy();
        }
    } quad;

    VulkanExample()
        : VulkanExampleBase(ENABLE_VALIDATION) {
        title = "Negative Viewport height";
        settings.overlay = true;
        // [POI] VK_KHR_MAINTENANCE1 is required for using negative viewport heights
        // Note: This is core as of Vulkan 1.1. So if you target 1.1 you don't have to explicitly enable this
        enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    }

    ~VulkanExample() {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        textures.CW.destroy();
        textures.CCW.destroy();
        quad.destroy();
    }

    void buildCommandBuffers() {
        vk::CommandBufferBeginInfo cmdBufInfo;

        vk::ClearValue clearValues[2];
        clearValues[0].color = defaultClearColor;
        clearValues[1].depthStencil = { 1.0f, 0 };

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = width;
        renderPassBeginInfo.renderArea.extent.height = height;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
            renderPassBeginInfo.framebuffer = frameBuffers[i];

            VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            // [POI] Viewport setup
            vk::Viewport viewport{};
            if (negativeViewport) {
                viewport.x = offsetx;
                // [POI] When using a negative viewport height, the origin needs to be adjusted too
                viewport.y = (float)height - offsety;
                viewport.width = (float)width;
                // [POI] Flip the sign of the viewport's height
                viewport.height = -(float)height;
            } else {
                viewport.x = offsetx;
                viewport.y = offsety;
                viewport.width = (float)width;
                viewport.height = (float)height;
            }
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            vk::Rect2D scissor{ width, height, 0, 0 };
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vk::DeviceSize offsets[1] = { 0 };

            // Render the quad with clock wise and counter clock wise indices, visibility is determined by pipeline settings

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.CW, 0, nullptr);
            vkCmdBindIndexBuffer(drawCmdBuffers[i], quad.indicesCW.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, quadType == 0 ? &quad.verticesYDown.buffer : &quad.verticesYUp.buffer, offsets);
            vkCmdDrawIndexed(drawCmdBuffers[i], 6, 1, 0, 0, 0);

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.CCW, 0, nullptr);
            vkCmdBindIndexBuffer(drawCmdBuffers[i], quad.indicesCCW.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(drawCmdBuffers[i], 6, 1, 0, 0, 0);

            drawUI(drawCmdBuffers[i]);

            vkCmdEndRenderPass(drawCmdBuffers[i]);

            VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
        }
    }

    void loadAssets() {
        textures.CW.loadFromFile(context, getAssetPath() + "textures/texture_orientation_cw_rgba.ktx", vk::Format::eR8G8B8A8Unorm);
        textures.CCW.loadFromFile(context, getAssetPath() + "textures/texture_orientation_ccw_rgba.ktx", vk::Format::eR8G8B8A8Unorm);

        // [POI] Create two quads with different Y orientations

        struct Vertex {
            float pos[3];
            float uv[2];
        };

        const float ar = (float)height / (float)width;

        // OpenGL style (y points upwards)
        std::vector<Vertex> verticesYPos = {
            { -1.0f * ar, 1.0f, 1.0f, 0.0f, 1.0f },
            { -1.0f * ar, -1.0f, 1.0f, 0.0f, 0.0f },
            { 1.0f * ar, -1.0f, 1.0f, 1.0f, 0.0f },
            { 1.0f * ar, 1.0f, 1.0f, 1.0f, 1.0f },
        };

        // Vulkan style (y points downwards)
        std::vector<Vertex> verticesYNeg = {
            { -1.0f * ar, -1.0f, 1.0f, 0.0f, 1.0f },
            { -1.0f * ar, 1.0f, 1.0f, 0.0f, 0.0f },
            { 1.0f * ar, 1.0f, 1.0f, 1.0f, 0.0f },
            { 1.0f * ar, -1.0f, 1.0f, 1.0f, 1.0f },
        };

        const vk::MemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VK_CHECK_RESULT(
            vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryPropertyFlags, &quad.verticesYUp, sizeof(Vertex) * 4, verticesYPos.data()));
        VK_CHECK_RESULT(
            vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryPropertyFlags, &quad.verticesYDown, sizeof(Vertex) * 4, verticesYNeg.data()));

        // [POI] Create two set of indices, one for counter clock wise, and one for clock wise rendering
        std::vector<uint32_t> indices = { 2, 1, 0, 0, 3, 2 };
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryPropertyFlags, &quad.indicesCCW, indices.size() * sizeof(uint32_t),
                                                   indices.data()));
        indices = { 0, 1, 2, 2, 3, 0 };
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryPropertyFlags, &quad.indicesCW, indices.size() * sizeof(uint32_t),
                                                   indices.data()));
    }

    void setupDescriptors() {
        std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings = { { vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment,
                                                                            0 } };
        vk::DescriptorSetLayoutCreateInfo descriptorLayoutCI{ setLayoutBindings };
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));
        vk::PipelineLayoutCreateInfo pipelineLayoutCI{ &descriptorSetLayout, 1 };
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

        vk::DescriptorPoolSize poolSize{ vk::DescriptorType::eCombinedImageSampler, 2 };
        vk::DescriptorPoolCreateInfo descriptorPoolCI{ 1, &poolSize, 2 };
        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool));

        vk::DescriptorSetAllocateInfo descriptorSetAI{ descriptorPool, &descriptorSetLayout, 1 };

        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAI, &descriptorSets.CW));
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAI, &descriptorSets.CCW));

        std::vector<vk::WriteDescriptorSet> writeDescriptorSets = {
            { descriptorSets.CW, vk::DescriptorType::eCombinedImageSampler, 0, &textures.CW.descriptor },
            { descriptorSets.CCW, vk::DescriptorType::eCombinedImageSampler, 0, &textures.CCW.descriptor }
        };
        vkUpdateDescriptorSets(device, 2, &writeDescriptorSets[0], 0, nullptr);
    }

    void preparePipelines() {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }

        const std::vector<vk::DynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE };
        vk::PipelineColorBlendAttachmentState blendAttachmentState{ 0xf, VK_FALSE };
        vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{ 1, &blendAttachmentState };
        vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{ VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL };
        vk::PipelineViewportStateCreateInfo viewportStateCI{ 1, 1, 0 };
        vk::PipelineMultisampleStateCreateInfo multisampleStateCI{ VK_SAMPLE_COUNT_1_BIT, 0 };
        vk::PipelineDynamicStateCreateInfo dynamicStateCI{ dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0 };

        vk::PipelineRasterizationStateCreateInfo rasterizationStateCI{};
        rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCI.lineWidth = 1.0f;
        rasterizationStateCI.cullMode = VK_CULL_MODE_NONE + cullMode;
        rasterizationStateCI.frontFace = windingOrder == 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Vertex bindings and attributes
        std::vector<vk::VertexInputBindingDescription> vertexInputBindings = {
            { 0, sizeof(float) * 5, vk::VertexInputRate::eVertex },
        };
        std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes = {
            { 0, 0, vk::Format::eR32G32B32sFloat, 0 },               // Position
            { 0, 1, vk::Format::eR32G32sFloat, sizeof(float) * 3 },  // uv
        };
        vk::PipelineVertexInputStateCreateInfo vertexInputState;
        vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
        vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
        vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

        vk::GraphicsPipelineCreateInfo pipelineCreateInfoCI{ pipelineLayout, renderPass, 0 };
        //pipelineCreateInfoCI.pVertexInputState = &emptyInputState;
        pipelineCreateInfoCI.pVertexInputState = &vertexInputState;
        pipelineCreateInfoCI.pInputAssemblyState = &inputAssemblyStateCI;
        pipelineCreateInfoCI.pRasterizationState = &rasterizationStateCI;
        pipelineCreateInfoCI.pColorBlendState = &colorBlendStateCI;
        pipelineCreateInfoCI.pMultisampleState = &multisampleStateCI;
        pipelineCreateInfoCI.pViewportState = &viewportStateCI;
        pipelineCreateInfoCI.pDepthStencilState = &depthStencilStateCI;
        pipelineCreateInfoCI.pDynamicState = &dynamicStateCI;

        const std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
            loadShader(getAssetPath() + "shaders/negativeviewportheight/quad.vert.spv", vk::ShaderStageFlagBits::eVertex),
            loadShader(getAssetPath() + "shaders/negativeviewportheight/quad.frag.spv", vk::ShaderStageFlagBits::eFragment)
        };

        pipelineCreateInfoCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfoCI.pStages = shaderStages.data();

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfoCI, nullptr, &pipeline));
    }

    void draw() {
        VulkanExampleBase::prepareFrame();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        VulkanExampleBase::submitFrame();
    }

    void prepare() {
        VulkanExampleBase::prepare();

        setupDescriptors();
        preparePipelines();
        buildCommandBuffers();
        prepared = true;
    }

    virtual void render() {
        if (!prepared)
            return;
        draw();
    }

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay) {
        if (overlay->header("Scene")) {
            overlay->text("Quad type");
            if (overlay->comboBox("##quadtype", &quadType, { "VK (y negative)", "GL (y positive)" })) {
                buildCommandBuffers();
            }
        }

        if (overlay->header("Viewport")) {
            if (overlay->checkBox("Negative viewport height", &negativeViewport)) {
                buildCommandBuffers();
            }
            if (overlay->sliderInt("offfset x", &offsetx, -(int32_t)width, (int32_t)width)) {
                buildCommandBuffers();
            }
            if (overlay->sliderInt("offfset y", &offsety, -(int32_t)height, (int32_t)height)) {
                buildCommandBuffers();
            }
        }
        if (overlay->header("Pipeline")) {
            overlay->text("Winding order");
            if (overlay->comboBox("##windingorder", &windingOrder, { "clock wise", "counter clock wise" })) {
                preparePipelines();
            }
            overlay->text("Cull mode");
            if (overlay->comboBox("##cullmode", &cullMode, { "none", "front face", "back face" })) {
                preparePipelines();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()