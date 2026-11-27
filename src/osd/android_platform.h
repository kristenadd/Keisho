// ██╗  ██╗███████╗██╗███████╗██╗  ██╗ ██████╗
// ██║ ██╔╝██╔════╝██║██╔════╝██║  ██║██╔═══██╗
// █████╔╝ █████╗  ██║███████╗███████║██║   ██║
// ██╔═██╗ ██╔══╝  ██║╚════██║██╔══██║██║   ██║
// ██║  ██╗███████╗██║███████║██║  ██║╚██████╔╝
// ╚═╝  ╚═╝╚══════╝╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝
//            継 承  ( k e i s h ō )
//
#pragma once

#include <string>

namespace sm2::osd {

/// Extract bundled APK assets into internal storage and return its path.
[[nodiscard]] std::string android_prepare_assets();

[[nodiscard]] std::string android_assets_dir();

/// App-specific external files directory, falling back to internal storage.
[[nodiscard]] std::string android_external_root_dir();

[[nodiscard]] std::string android_shared_storage_dir();

[[nodiscard]] bool android_has_storage_access();

void android_request_storage_access();

}  // namespace sm2::osd
