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
// The system's own folder picker, where the platform has one an app must use
// (iOS: the Files app's picker). Elsewhere the overlay's own browser is used.

#pragma once

#include <functional>
#include <string>

struct SDL_Window;

namespace sm2 {
struct Config;
}

namespace sm2::osd {

/// Whether pick_folder() shows a system picker on this platform.
[[nodiscard]] bool native_folder_picker_available();

/// Show the system folder picker over `window`. `done` runs later on the main
/// thread (from SDL's event pump) with the chosen folder's path, or an empty
/// string if the picker was cancelled. On iOS the folder may lie outside the
/// app (iCloud Drive, another app's storage): access is kept for this session
/// and remembered under `key`, a Config key such as "model3_rom_dir", so that
/// restore_folder_access() can reopen it on the next launch.
void pick_folder(SDL_Window* window, const std::string& key, std::function<void(std::string)> done);

/// Reopen the folders picked in earlier sessions and point their Config
/// fields at where they are now. A no-op where there is no system picker.
void restore_folder_access(Config* config);

}  // namespace sm2::osd
