// iPhone and iPad: keep SDL's view the size of its window. SDL sizes the view
// from -[UIApplication statusBarOrientation], which a scene-based app launched
// in landscape on iPadOS 26 and later can still report as portrait; SDL then
// turns the landscape window's frame portrait, and the overlay and the game are
// drawn into a portrait rectangle in the top-left of the screen. No-op
// elsewhere.
#pragma once

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

struct SDL_Window;

namespace sm2::osd {

#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_VISION

/// Give `window`'s root view its UIWindow's bounds when they differ. Cheap
/// enough to call every frame; a change reaches SDL as a resize.
void fit_view_to_window(SDL_Window* window);

#else

inline void fit_view_to_window(SDL_Window*) {}

#endif

}  // namespace sm2::osd
