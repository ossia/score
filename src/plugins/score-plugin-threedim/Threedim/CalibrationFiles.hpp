#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Threedim
{
//! A projector calibration as Structured Light Calibration saves it:
//! `base.xyz.pfm` + `base.valid.pfm`, `base.json`, and when measured
//! `base.lum.pfm` (white - black) and `base.black.pfm`.
struct CalibrationFile
{
  std::string base;
  int width{}, height{};
  //! RGBA, rows top to bottom.
  std::vector<float> xyz;
  std::array<float, 4> intrinsics{};
  std::array<float, 16> pose{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool hasPose{};
  bool viewerMode{};
  //! RGB, empty when not measured.
  std::vector<float> lum, black;
  std::string error;

  bool ok() const noexcept
  {
    return width > 0 && height > 0 && xyz.size() == std::size_t(width) * height * 4;
  }
  bool hasModel() const noexcept { return hasPose && intrinsics[0] > 0.f && intrinsics[1] > 0.f; }
};

CalibrationFile loadCalibration(const std::string& path);

//! Writes the maps and the json of `c` under `c.base`; extra json keys are
//! merged from the original file when there is one at `copyJsonFrom`.
bool saveCalibration(const CalibrationFile& c, const std::string& copyJsonFrom = {});

//! Splits a list of paths on new lines, ';' or '|'.
std::vector<std::string> splitPaths(std::string_view list);

//! Splits a list of numbers on ',', ';' or spaces.
std::vector<float> splitNumbers(std::string_view list);
}
