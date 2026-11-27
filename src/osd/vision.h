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
// the 3D. PlayStation VR2 Sense controllers aim the light guns. Every call is
// a no-op on other platforms.
#pragma once

#include "render/backend.h"

#include <array>

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

struct SDL_Window;

namespace sm2::osd::vision {

/// A spatial controller (PlayStation VR2 Sense) used as a light gun.
struct SpatialGun {
    bool  present = false;  ///< A controller drives this player.
    bool  aimed   = false;  ///< Tracked and pointing at the window's plane.
    float x       = -1.0f;  ///< 0..1 across the window; outside is off the window.
    float y       = -1.0f;
    bool  right_hand = false;  ///< The right-hand controller.
    bool  trigger    = false;
    bool  grip       = false;
    bool  primary    = false;  ///< Cross on the right controller, Square on the left: Start.
    bool  secondary  = false;  ///< Circle on the right controller, Triangle on the left: Coin.
    bool  stick_press = false;  ///< The thumbstick pushed in (R3 / L3).
    bool  menu       = false;  ///< The small button: Options on the right, Create (Share) on the left.
    float stick_x    = 0.0f;   ///< Thumbstick, -1..1, right and up positive.
    float stick_y    = 0.0f;

    [[nodiscard]] bool any_button() const { return trigger || grip || primary || secondary || menu; }
};

/// A light-gun crosshair to draw on the stereo layer, which covers the flat
/// overlay while 3D is on.
struct Crosshair {
    float x      = 0.5f;   ///< Centre, 0..1 across the window.
    float y      = 0.5f;
    float radius = 0.02f;  ///< Fraction of the window's height.
    float r = 0.0f, g = 1.0f, b = 0.0f;
};

#if defined(__APPLE__) && TARGET_OS_VISION

/// Hold the window to `aspect` (width / height), keeping its area; 0 lets it
/// take any shape. Cheap to call every frame: only a change is sent.
void set_window_aspect(SDL_Window* window, float aspect);

/// Show `frame` (side by side, left eye in the left half) over the window at
/// `aspect`, one half to each eye, or hide the stereo layer when `show` is
/// false or the frame is empty. Copies the frame before returning, so call it
/// between the backend's begin_frame() and end_frame().
void present_stereo(SDL_Window* window, const render::StereoFrame& frame, bool show, float aspect);

/// The spatial controllers, by player: the right-hand controller is player 1
/// and the left-hand one player 2, or a lone controller is player 1. Starts
/// tracking (and asks for permission) the first time a controller is seen.
[[nodiscard]] std::array<SpatialGun, 2> spatial_guns(SDL_Window* window);

/// The crosshairs to show over the stereo picture this frame (up to 2); the
/// rest are hidden. They show only while the stereo layer does.
void set_stereo_crosshairs(SDL_Window* window, const Crosshair* marks, int count);

/// A Sense controller's stick was pressed in (R3 / L3) since the last call,
/// which opens the menu (and pauses) like a gamepad's Home button. The PS
/// button does not reach apps.
[[nodiscard]] bool spatial_menu_pressed(SDL_Window* window);

#else

inline void set_window_aspect(SDL_Window*, float) {}
inline void present_stereo(SDL_Window*, const render::StereoFrame&, bool, float) {}
inline std::array<SpatialGun, 2> spatial_guns(SDL_Window*) { return {}; }
inline void set_stereo_crosshairs(SDL_Window*, const Crosshair*, int) {}
inline bool spatial_menu_pressed(SDL_Window*) { return false; }

#endif

}  // namespace sm2::osd::vision
