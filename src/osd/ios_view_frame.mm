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

#include "osd/ios_view_frame.h"

#if TARGET_OS_IPHONE && !TARGET_OS_VISION

#include <SDL3/SDL.h>

#import <UIKit/UIKit.h>

namespace sm2::osd {

void fit_view_to_window(SDL_Window* window)
{
    if (window == nullptr) {
        return;
    }
    UIWindow* ui = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                              SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    UIView* view = ui.rootViewController.view;
    if (view == nil || CGRectIsEmpty(ui.bounds) || CGRectEqualToRect(view.frame, ui.bounds)) {
        return;
    }
    // The layout this sets off ends in SDL's -viewDidLayoutSubviews, which
    // sends the window its true size.
    view.frame = ui.bounds;
    [view layoutIfNeeded];
}

}  // namespace sm2::osd

#endif
