#pragma once
#include <Gfx/Window/WindowSettings.hpp>
#include <Threedim/CameraLens.hpp>
#include <Threedim/LensConversion.hpp>
#include <Threedim/RoomGeometry.hpp>
#include <Threedim/StructuredLightCore.hpp>

#include <QCoreApplication>
#include <QRectF>
#include <QStringList>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace Threedim
{
//! What follows the capture of several outputs by structured light, off the
//! GUI thread: the results saved under their outputs' names, the viewer
//! camera's lens estimated, its maps aligned to an earlier run and completed
//! from their planes, its pose in the room refined, the room points and the
//! equirectangular warps written, then the blend. Each step is a function of
//! its own; run() chains them, and stops between two when cancelled.
class CalibrationPostProcess
{
  Q_DECLARE_TR_FUNCTIONS(Threedim::CalibrationPostProcess)
public:
  using Results = std::vector<std::shared_ptr<const StructuredLightCore::Result>>;

  enum class Geometry
  {
    Directions,
    Box,
    Model
  };

  //! An output of this run whose earlier calibration defines the frame.
  struct Anchor
  {
    int projector{};
    //! The earlier name.json.
    QString calibration;
  };

  struct Options
  {
    //! The calibration base of each result.
    QStringList names;
    //! The device output of each result, for the log; empty = 0, 1, ...
    std::vector<int> outputs;
    bool viewer{};
    CameraLens lens;
    float fov{60.f};
    std::array<float, 4> cameraIntrinsics{};
    std::array<float, 16> worldFromCamera{1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
    //! Emissive screens: name.json says so, and the blend leaves them out.
    bool screens{};
    bool completePlanes{};
    bool estimateLens{};
    double planeThresholdArcmin{6.};
    Geometry geometry{Geometry::Directions};
    //! Front, back, left, right, floor, ceiling distances from the camera (m).
    std::array<float, 6> box{};
    QString model;
    std::array<float, 3> cameraPosition{};
    std::array<float, 3> cameraRotation{};
    bool refinePose{};
    std::vector<Anchor> anchors;
    Gfx::CanvasProjection projection{Gfx::CanvasProjection::None};
    std::vector<QRectF> sourceRects;
    bool blend{};
    bool blackCompensation{};
    //! The name.json the blend runs on; empty = those of this run.
    QStringList blendCalibrations;
  };

  struct Outcome
  {
    bool ok{};
    QString status;
    QStringList log;
    std::optional<CameraLens> lens;
    //! Position, Euler degrees.
    std::optional<std::array<float, 6>> pose;
  };

  //! Everything, from a fresh capture (its results are saved first) or from
  //! the saved calibrations of `names` (load()).
  //! `progress` goes from 0 to 1 over the steps.
  static Outcome
  run(const Results& results, const Options& o, bool fresh,
      const std::atomic_bool* cancel = nullptr, std::atomic<float>* progress = nullptr);

  //! The saved calibrations of `names`, from their measured maps when there
  //! are: what a recompute starts from. Null where there is none.
  static Results load(const QStringList& names, QStringList& log);

  //! Writes each result under its name, with name.json's "display".
  static bool saveResults(const Results& results, const Options& o, QStringList& log);

  //! The room the viewer camera's directions are cast into; empty for none.
  static RoomGeometry room(const Options& o, QStringList& log);

  //! A fresh capture's measured map and camera, kept as name.measured.*; a
  //! recompute reads them back.
  struct Measured
  {
    std::vector<StructuredLightCore::Result> results;
    std::vector<std::optional<MeasuredCamera>> cameras;
  };
  static Measured measure(const Results& results, const Options& o, bool fresh);

  //! The radial distortion for which the walls come out flat, if asked.
  static std::optional<CameraLens>
  estimateLens(const Measured& m, const Options& o, QStringList& log);
  //! Each map re-expressed through `lens` and the camera intrinsics.
  static void convertLens(Measured& m, const Options& o, const CameraLens& lens);
  //! Turns every map by the rotation that takes the anchors onto their
  //! earlier calibration.
  static void align(Measured& m, const Options& o, QStringList& log);
  //! The direction maps to cast, completed from their planes when asked (and
  //! the completed ones saved).
  static std::vector<std::vector<float>>
  completePlanes(const Measured& m, const Options& o, QStringList& log);
  static std::optional<std::array<float, 6>> refinePose(
      const Measured& m, const std::vector<std::vector<float>>& directions,
      const RoomGeometry& room, const Options& o, QStringList& log);
  //! Points and warp maps; those no longer made are taken out of name.json.
  static void writeMaps(
      const Measured& m, const std::vector<std::vector<float>>& directions,
      const RoomGeometry& room, const std::array<float, 6>& pose, const Options& o,
      QStringList& log);
  static void blend(const Options& o, QStringList& log);

  static QString label(const Options& o, int k);
};
}
