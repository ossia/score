#pragma once
#include <Threedim/CalibrationFiles.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{

class RegisterCalibrations
{
public:
  halp_meta(name, "Register Calibrations")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "register_calibrations")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "Bring calibrations made from another camera position into the reference world "
      "frame. A projector calibrated from both positions gives, for each of its pixels, "
      "the same surface point in both frames; the rigid (or similarity) transform "
      "between them is solved by least squares (Umeyama 1991, doi:10.1109/34.88573; "
      "Horn 1987, doi:10.1364/JOSAA.4.000629) inside RANSAC (Fischler & Bolles 1981, "
      "doi:10.1145/358669.358692), then applied to the XYZ maps and poses of the "
      "calibrations to move, written with a suffix.")
  halp_meta(uuid, "5e9b2c7a-41d8-4f63-b0a2-8c6d1e7f3b95")

  struct ins
  {
    //! The shared projector, in the reference frame.
    halp::lineedit<"Reference calibration", ""> reference;
    //! The same projector, in the frame to move.
    halp::lineedit<"Other calibration", ""> other;
    //! Calibrations in the frame of Other calibration; it is moved too when listed.
    halp::lineedit<"Calibrations to move", ""> move;
    halp::lineedit<"Suffix", "_reg"> suffix;
    halp::hslider_f32<"Inlier threshold (m)", halp::range{0.001, 0.5, 0.02}> threshold;
    halp::toggle<"Allow scale"> scale;
    halp::spinbox_i32<"Pixel step", halp::irange{1, 64, 4}> step;
    halp::impulse_button<"Register"> run;
  } inputs;

  struct
  {
    //! Column-major, reference frame from other frame.
    halp::val_port<"Transform", std::array<float, 16>> transform{
        .value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    halp::val_port<"RMS (m)", float> rms;
    halp::val_port<"Inliers", int> inliers;
    halp::val_port<"Written", std::vector<std::string>> written;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
  } outputs;

  struct Result
  {
    bool ok{};
    std::array<float, 16> transform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    double rms{};
    double scale{1.};
    int inliers{}, correspondences{};
    std::string status;
  };

  //! Transform taking `other`'s frame to `reference`'s, from their common pixels.
  static Result solve(
      const CalibrationFile& reference, const CalibrationFile& other, double threshold,
      bool allowScale, int step);
  //! Applies a reference-from-other transform to a calibration.
  static void apply(CalibrationFile& c, const std::array<float, 16>& transform);

  void operator()();
};
}
