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
// Apple Vision Pro: the app is one window in the Shared Space. Its shape is held
// to the picture's aspect, so resizing it from the corner keeps 4:3 or 16:9,
// and with stereo on, a RealityKit layer over it shows each eye its own view of
// the 3D. Every call is a no-op on other platforms.
#pragma once

#include "render/backend.h"

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

struct SDL_Window;

namespace sm2::osd::vision {

#if defined(__APPLE__) && TARGET_OS_VISION

/// Hold the window to `aspect` (width / height), keeping its area; 0 lets it
/// take any shape. Cheap to call every frame: only a change is sent.
void set_window_aspect(SDL_Window* window, float aspect);

/// Show `frame` (side by side, left eye in the left half) over the window at
/// `aspect`, one half to each eye, or hide the stereo layer when `show` is
/// false or the frame is empty. Copies the frame before returning, so call it
/// between the backend's begin_frame() and end_frame().
void present_stereo(SDL_Window* window, const render::StereoFrame& frame, bool show, float aspect);

#else

inline void set_window_aspect(SDL_Window*, float) {}
inline void present_stereo(SDL_Window*, const render::StereoFrame&, bool, float) {}

#endif

}  // namespace sm2::osd::vision
