#pragma once

namespace score::gfx
{

/**
 * @brief How to resize a texture to adapt it to a viewport.
 */
enum ScaleMode {
  Original, // 1 pixel in = 1 pixel out
  BlackBars, // Keep aspect ratio and scale so that the whole picture is visible
  Fill, // Keep aspect ratio and scale so that the whole screen is filled
  Stretch // Stretch to the viewport size
};

}
