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
//
// The side-by-side stereo frame for Apple Vision Pro: both eyes' finished
// native frames copied into one image per frame in flight, each exported as an
// MTLTexture (VK_EXT_metal_objects) for RealityKit to show, one eye each
// (osd/vision_stereo.swift).
#pragma once

#include "render/backend.h"
#include "render/vk/context.h"

#include <array>

using VmaAllocation = struct VmaAllocation_T*;

namespace sm2::render::vk {

class StereoOut {
public:
    StereoOut() = default;
    ~StereoOut() { shutdown(); }

    StereoOut(const StereoOut&)            = delete;
    StereoOut& operator=(const StereoOut&) = delete;

    /// Whether this device can export images to Metal at all.
    [[nodiscard]] static bool available(const Context& context) { return context.metal_objects(); }

    /// One eye is `eye_width` x `eye_height`, the composite target's size.
    [[nodiscard]] bool init(Context& context, u32 eye_width, u32 eye_height);
    void shutdown();
    [[nodiscard]] bool ready() const { return m_context != nullptr; }
    [[nodiscard]] u32 eye_width() const { return m_eye_width; }
    [[nodiscard]] u32 eye_height() const { return m_eye_height; }

    /// After Context::begin_frame(): this slot's image, if it was written,
    /// holds a frame the GPU has finished. Until the frame's commands are
    /// submitted it stays intact.
    [[nodiscard]] StereoFrame finished() const;

    /// Copy both eyes into this frame's image. Both images must be in
    /// COLOR_ATTACHMENT_OPTIMAL, and are left that way. `right` may be the left
    /// image again, for a frame that had no right eye (software renderer).
    void record(VkImage left, VkImage right);

private:
    struct Target {
        VkImage       image      = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        void*         texture    = nullptr;  ///< id<MTLTexture>, not retained
        bool          written    = false;
    };

    Context* m_context    = nullptr;
    u32      m_eye_width  = 0;
    u32      m_eye_height = 0;
    std::array<Target, Context::kFramesInFlight> m_targets{};
};

}  // namespace sm2::render::vk
