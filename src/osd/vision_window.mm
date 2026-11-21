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

#import <UIKit/UIKit.h>

#include <cmath>

// vision_stereo.swift.
extern "C" void sm2_vision_stereo_present(void* window, void* texture, unsigned width, unsigned height,
                                          bool show, float aspect);

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

}  // namespace

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
