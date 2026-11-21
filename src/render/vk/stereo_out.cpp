// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
// A Sega Model 1, 2 & 3 arcade emulator.
// Copyright (c) 2025+ Daniel Martin (dmanlfc)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This header must not be removed. Keisho is licensed under the GNU General
// Public License, version 3 or later, with additional terms under section 7 of
// that licence. See LICENSE and NOTICE.

#include "render/vk/stereo_out.h"

#include "core/log.h"
#include "render/vk/vk_common.h"

#include <vk_mem_alloc.h>

#if defined(__APPLE__)
#  include <vulkan/vulkan_metal.h>
#endif

namespace sm2::render::vk {

bool StereoOut::init(Context& context, u32 eye_width, u32 eye_height)
{
    shutdown();
#if defined(__APPLE__)
    if (!context.metal_objects()) {
        return false;
    }
    const auto export_objects = reinterpret_cast<PFN_vkExportMetalObjectsEXT>(
        vkGetDeviceProcAddr(context.device(), "vkExportMetalObjectsEXT"));
    if (export_objects == nullptr) {
        return false;
    }

    for (Target& t : m_targets) {
        VkExportMetalObjectCreateInfoEXT exportable{};
        exportable.sType            = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECT_CREATE_INFO_EXT;
        exportable.exportObjectType = VK_EXPORT_METAL_OBJECT_TYPE_METAL_TEXTURE_BIT_EXT;

        VkImageCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.pNext         = &exportable;
        info.imageType     = VK_IMAGE_TYPE_2D;
        // The native frame's bytes, which are sRGB-encoded, copied unchanged
        // into an image that says so: RealityKit then decodes them for its
        // linear blending and the colours come out as the window shows them.
        info.format        = VK_FORMAT_R8G8B8A8_SRGB;
        info.extent        = {eye_width * 2, eye_height, 1};
        info.mipLevels     = 1;
        info.arrayLayers   = 1;
        info.samples       = VK_SAMPLE_COUNT_1_BIT;
        info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        info.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo alloc{};
        alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        if (vmaCreateImage(context.allocator(), &info, &alloc, &t.image, &t.allocation, nullptr) != VK_SUCCESS) {
            SM2_WARN("stereo: could not create the side-by-side image");
            m_context = &context;
            shutdown();
            return false;
        }

        VkExportMetalTextureInfoEXT texture{};
        texture.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT;
        texture.image = t.image;
        texture.plane = VK_IMAGE_ASPECT_PLANE_0_BIT;
        VkExportMetalObjectsInfoEXT objects{};
        objects.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
        objects.pNext = &texture;
        export_objects(context.device(), &objects);
        t.texture = reinterpret_cast<void*>(texture.mtlTexture);
        t.written = false;
        if (t.texture == nullptr) {
            SM2_WARN("stereo: MoltenVK did not export the side-by-side image");
            m_context = &context;
            shutdown();
            return false;
        }
    }
    m_context    = &context;
    m_eye_width  = eye_width;
    m_eye_height = eye_height;
    SM2_INFO("stereo: side-by-side frame %ux%u", eye_width * 2, eye_height);
    return true;
#else
    static_cast<void>(context);
    static_cast<void>(eye_width);
    static_cast<void>(eye_height);
    return false;
#endif
}

void StereoOut::shutdown()
{
    if (m_context == nullptr) {
        return;
    }
    for (Target& t : m_targets) {
        if (t.image != VK_NULL_HANDLE) {
            vmaDestroyImage(m_context->allocator(), t.image, t.allocation);
        }
        t = Target{};
    }
    m_context    = nullptr;
    m_eye_width  = 0;
    m_eye_height = 0;
}

StereoFrame StereoOut::finished() const
{
    if (m_context == nullptr) {
        return {};
    }
    const Target& t = m_targets[m_context->frame_index()];
    if (!t.written) {
        return {};
    }
    return StereoFrame{t.texture, m_eye_width * 2, m_eye_height};
}

void StereoOut::record(VkImage left, VkImage right)
{
    if (m_context == nullptr) {
        return;
    }
    const VkCommandBuffer cmd = m_context->cmd();
    Target&               t   = m_targets[m_context->frame_index()];

    // The previous contents were copied out to Metal before this frame's
    // commands were submitted, so nothing needs preserving.
    record_image_barrier(cmd, t.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, 0,
                         VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    const auto copy_eye = [&](VkImage source, u32 x) {
        record_image_barrier(cmd, source, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT,
                             VK_ACCESS_2_TRANSFER_READ_BIT);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstOffset      = {static_cast<s32>(x), 0, 0};
        region.extent         = {m_eye_width, m_eye_height, 1};
        vkCmdCopyImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        record_image_barrier(cmd, source, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, 0,
                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    };
    copy_eye(left, 0);
    copy_eye(right, m_eye_width);

    // Metal reads it after this frame's fence; the layout is only for Vulkan.
    record_image_barrier(cmd, t.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
    t.written = true;
}

}  // namespace sm2::render::vk
