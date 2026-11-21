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
// The launch picture over the window, laid out as data/launch/
// LaunchScreen.storyboard is: the gradient filling it, the logo the largest
// square within 80% of each side, centred.

#include "osd/launch_splash.h"

#include <SDL3/SDL.h>

#include <algorithm>

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

namespace sm2::osd {

namespace {

UIView*        g_splash = nil;
CFTimeInterval g_shown   = 0;

#if TARGET_OS_VISION
// visionOS shows its own placeholder, the app's small icon on glass, while the
// app loads, and the first frame follows the window almost at once: the launch
// picture stays up this long so that it is seen. The emulator starts beneath it.
constexpr CFTimeInterval kMinimumShown = 1.25;
#else
// iOS has shown the launch storyboard while the app loaded.
constexpr CFTimeInterval kMinimumShown = 0.0;
#endif

}  // namespace

void show_launch_splash(SDL_Window* window)
{
    if (window == nullptr) {
        return;
    }
    UIWindow* ui = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                              SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    if (g_splash != nil) {
        if (g_splash.superview == ui) {
            return;
        }
        // A window made again (another renderer tried) gets a picture of its own.
        [g_splash removeFromSuperview];
        g_splash = nil;
    }
    UIImage* background = [UIImage imageNamed:@"LaunchBackground"];
    UIImage* logo       = [UIImage imageNamed:@"LaunchLogo"];
    if (ui == nil || background == nil || logo == nil) {
        return;
    }

    UIView* splash = [[UIView alloc] initWithFrame:ui.bounds];
    splash.autoresizingMask       = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    splash.userInteractionEnabled = NO;

    UIImageView* fill = [[UIImageView alloc] initWithImage:background];
    fill.contentMode = UIViewContentModeScaleToFill;
    fill.translatesAutoresizingMaskIntoConstraints = NO;
    [splash addSubview:fill];

    UIImageView* art = [[UIImageView alloc] initWithImage:logo];
    art.contentMode = UIViewContentModeScaleAspectFit;
    art.translatesAutoresizingMaskIntoConstraints = NO;
    [splash addSubview:art];

    NSLayoutConstraint* wide = [art.widthAnchor constraintEqualToAnchor:splash.widthAnchor multiplier:0.8];
    NSLayoutConstraint* tall = [art.heightAnchor constraintEqualToAnchor:splash.heightAnchor multiplier:0.8];
    wide.priority = UILayoutPriorityDefaultHigh;
    tall.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [fill.leadingAnchor constraintEqualToAnchor:splash.leadingAnchor],
        [fill.trailingAnchor constraintEqualToAnchor:splash.trailingAnchor],
        [fill.topAnchor constraintEqualToAnchor:splash.topAnchor],
        [fill.bottomAnchor constraintEqualToAnchor:splash.bottomAnchor],
        [art.centerXAnchor constraintEqualToAnchor:splash.centerXAnchor],
        [art.centerYAnchor constraintEqualToAnchor:splash.centerYAnchor],
        [art.widthAnchor constraintEqualToAnchor:art.heightAnchor],
        [art.widthAnchor constraintLessThanOrEqualToAnchor:splash.widthAnchor multiplier:0.8],
        [art.heightAnchor constraintLessThanOrEqualToAnchor:splash.heightAnchor multiplier:0.8],
        wide,
        tall,
    ]];

    [ui addSubview:splash];
    g_splash = splash;
    g_shown  = CACurrentMediaTime();
}

void hide_launch_splash()
{
    UIView* splash = g_splash;
    if (splash == nil) {
        return;
    }
    g_splash = nil;
    // SDL puts its view controller's view, with the Metal layer, into the
    // window after the picture went up: lift the picture back over it.
    [splash.superview bringSubviewToFront:splash];
    const CFTimeInterval wait = std::max(0.0, kMinimumShown - (CACurrentMediaTime() - g_shown));
    [UIView animateWithDuration:0.25
                          delay:wait
                        options:UIViewAnimationOptionCurveEaseInOut
        animations:^{
          splash.alpha = 0.0;
        }
        completion:^(BOOL) {
          [splash removeFromSuperview];
        }];
}

}  // namespace sm2::osd
