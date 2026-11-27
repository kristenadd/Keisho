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
// Apple Vision Pro: the window's shape, through its scene's geometry
// preferences, and the hand-off to the stereo layer (vision_stereo.swift).

#include "osd/vision.h"

#include "core/log.h"

#include <SDL3/SDL.h>

#import <GameController/GameController.h>
#import <UIKit/UIKit.h>

#include <algorithm>
#include <cmath>

// vision_stereo.swift.
extern "C" void sm2_vision_stereo_present(void* window, void* texture, unsigned width, unsigned height,
                                          bool show, float aspect);
extern "C" void sm2_vision_crosshairs(void* window, const float* values, int count);
extern "C" int sm2_vision_aim_poll(void* window, const void** controllers, float* values, int max);

namespace sm2::osd::vision {

namespace {

UIWindow* uikit_window(SDL_Window* window)
{
    if (window == nullptr) {
        return nil;
    }
    return (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                      SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
}

bool pressed(GCController* controller, NSString* name)
{
    id<GCButtonElement> button = [controller.input.buttons objectForKeyedSubscript:name];
    return button != nil && button.pressedInput.pressed;
}

}  // namespace

std::array<SpatialGun, 2> spatial_guns(SDL_Window* window)
{
    std::array<SpatialGun, 2> guns{};
    // Nothing to track (and no RealityKit view added) until one connects.
    bool any = false;
    for (GCController* controller in GCController.controllers) {
        if ([controller.productCategory isEqualToString:GCProductCategorySpatialController]) {
            any = true;
            break;
        }
    }
    UIWindow* ui = uikit_window(window);
    if (!any || ui == nil) {
        return guns;
    }

    constexpr int kMax = 4;
    const void*   handles[kMax] = {};
    float         values[kMax * 4] = {};
    const int     count = sm2_vision_aim_poll((__bridge void*)ui, handles, values, kMax);

    // Right hand plays player 1, left hand player 2; an unhanded one takes
    // whichever is free, and a lone controller is always player 1.
    int order[kMax] = {};
    int slot[kMax]  = {-1, -1, -1, -1};
    for (int i = 0; i < count; ++i) {
        order[i] = i;
    }
    std::sort(order, order + count, [&](int a, int b) {
        const auto rank = [&](int i) { return values[i * 4] == 2.0F ? 0 : values[i * 4] == 1.0F ? 2 : 1; };
        return rank(a) < rank(b);
    });
    bool taken[2] = {false, false};
    for (int k = 0; k < count && k < 2; ++k) {
        const int i = order[k];
        int       p = (count > 1 && values[i * 4] == 1.0F) ? 1 : 0;
        if (taken[p]) {
            p = 1 - p;
        }
        taken[p] = true;
        slot[i]  = p;
    }
    for (int i = 0; i < count; ++i) {
        if (slot[i] < 0 || handles[i] == nullptr) {
            continue;
        }
        GCController* controller = (__bridge GCController*)handles[i];
        SpatialGun&   g          = guns[static_cast<size_t>(slot[i])];
        g.present   = true;
        g.aimed     = values[i * 4 + 1] != 0.0F;
        g.x         = values[i * 4 + 2];
        g.y         = values[i * 4 + 3];
        g.trigger   = pressed(controller, GCInputTrigger);
        g.grip      = pressed(controller, GCInputGripButton);
        g.right_hand = values[i * 4] == 2.0F;
        g.primary    = pressed(controller, GCInputButtonA) || pressed(controller, GCInputButtonX);
        g.secondary  = pressed(controller, GCInputButtonB) || pressed(controller, GCInputButtonY);
        g.stick_press = pressed(controller, GCInputThumbstickButton);
        g.menu       = pressed(controller, GCInputButtonMenu) || pressed(controller, GCInputButtonOptions)
                    || pressed(controller, GCInputButtonShare);
        // The live input reports the stick as a direction pad.
        id<GCDirectionPadElement> stick = [controller.input.dpads objectForKeyedSubscript:GCInputThumbstick];
        if (stick != nil) {
            g.stick_x = stick.xAxis.value;
            g.stick_y = stick.yAxis.value;
        }
    }
    return guns;
}

bool spatial_menu_pressed(SDL_Window* window)
{
    static bool was_down = false;
    bool        down     = false;
    const std::array<SpatialGun, 2> guns = spatial_guns(window);
    for (const SpatialGun& g : guns) {
        if (g.present && g.stick_press) {
            down = true;
        }
    }
    const bool press = down && !was_down;
    was_down         = down;
    return press;
}

void set_stereo_crosshairs(SDL_Window* window, const Crosshair* marks, int count)
{
    static int shown = 0;
    if (count == 0 && shown == 0) {
        return;  // nothing to hide
    }
    shown = count;
    UIWindow* ui = uikit_window(window);
    if (ui == nil) {
        return;
    }
    float values[2 * 6] = {};
    count               = std::min(count, 2);
    for (int i = 0; i < count; ++i) {
        const Crosshair& m = marks[i];
        const float      v[6] = {m.x, m.y, m.radius, m.r, m.g, m.b};
        std::copy(v, v + 6, values + i * 6);
    }
    sm2_vision_crosshairs((__bridge void*)ui, values, count);
}

void set_window_aspect(SDL_Window* window, float aspect)
{
    static float sent = -1.0F;
    UIWindow*    ui   = uikit_window(window);
    UIWindowScene* scene = ui.windowScene;
    if (scene == nil || aspect == sent) {
        return;
    }
    sent = aspect;

    UIWindowSceneGeometryPreferencesVision* prefs = nil;
    if (aspect > 0.0F) {
        // Keep the area the person chose and change only the shape; uniform
        // resizing then keeps that shape as the corner is dragged.
        const CGSize now  = ui.bounds.size;
        const double area = std::max(now.width * now.height, 640.0 * 480.0);
        const double w    = std::sqrt(area * aspect);
        prefs = [[UIWindowSceneGeometryPreferencesVision alloc] initWithSize:CGSizeMake(w, w / aspect)];
        prefs.resizingRestrictions = UIWindowSceneResizingRestrictionsUniform;
    } else {
        prefs = [[UIWindowSceneGeometryPreferencesVision alloc] init];
        prefs.resizingRestrictions = UIWindowSceneResizingRestrictionsFreeform;
    }
    [scene requestGeometryUpdateWithPreferences:prefs
                                   errorHandler:^(NSError* error) {
                                       SM2_WARN("visionOS: the window kept its shape: %s",
                                                error.localizedDescription.UTF8String);
                                   }];
}

void present_stereo(SDL_Window* window, const render::StereoFrame& frame, bool show, float aspect)
{
    static bool shown = false;
    const bool  want  = show && frame.texture != nullptr;
    if (!want && !shown) {
        return;  // nothing on screen to hide
    }
    shown = want;
    sm2_vision_stereo_present((__bridge void*)uikit_window(window), want ? frame.texture : nullptr,
                              frame.width, frame.height, want, aspect);
}

}  // namespace sm2::osd::vision
