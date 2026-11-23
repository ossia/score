#pragma once
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CameraLens.hpp>
#include <Threedim/StructuredLight.hpp>

#include <QJsonObject>
#include <QString>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{
//! What Structured Light Calibration computes from the frames of one
//! projector, independent of how the patterns are shown and the frames reach
//! it, and the files it reads and writes.
struct StructuredLightCore
{
  enum Mode
  {
    DepthCamera,
    ViewerCamera
  };
  enum Surface
  {
    Planes,
    SmoothMeasured,
    RawMeasured
  };
  enum IntrinsicsMode
  {
    SolveIntrinsics,
    LockIntrinsics,
    LockToLoaded
  };
  enum IdlePattern
  {
    Black,
    TestGrid,
    White
  };

  //! What a run (or a loaded file) produced for one projector.
  struct Result
  {
    bool ok{};
    std::string status;
    int width{}, height{};
    std::vector<float> xyz;
    int camWidth{}, camHeight{};
    //! The camera's intrinsics and lens the viewing directions were made with.
    std::array<float, 4> cameraIntrinsics{};
    CameraLens lens;
    std::vector<uint8_t> decoded;
    std::array<float, 4> intrinsics{};
    std::array<float, 16> pose{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float rms{};
    float coverage{};
    int mode{};
    //! n.xyz, d with n . p + d = 0, as ParametricSurface takes them.
    std::vector<std::array<float, 4>> planes;
    //! Per camera pixel, only when the job asked for them.
    std::vector<sl::DecodedPixel> pixels;
    //! RGBA per projector pixel, w = valid; empty without RGB frames.
    std::vector<float> lum, black;
    float cameraGamma{1.f};
    int projectorIndex{};
    sl::GraySequence seq;
    float phaseValid{};
    float ransac{};
    int intrinsicsMode{};
    bool intrinsicsLocked{};
    bool fromFile{};
    //! From the pattern shown to the camera seeing it, as measured.
    double latencyMs{};
  };

  //! The frames of one projector and how to compute them.
  struct Job
  {
    std::shared_ptr<const std::atomic_bool> cancel;
    //! A capture failure: the result is this status.
    std::string failed;
    //! Fed with linear luminance.
    sl::Decoder decoder;
    sl::GraySequence seq;
    std::vector<float> positions;
    std::vector<int> positionCount;
    std::vector<float> zMin, zMax;
    int positionFrames{};
    std::array<float, 16> worldFromCamera{};
    std::array<float, 4> cameraIntrinsics{};
    CameraLens lens;
    int mode{};
    float minSignal{}, depthJump{}, ransac{}, planeThreshold{0.02f};
    int intrinsicsMode{};
    bool lockK{};
    std::array<float, 4> lockedK{};
    std::string kNote;
    int minBits{}, holeRadius{}, surface{}, maxPlanes{2};
    //! Raw RGB per camera pixel of the white and black frames.
    std::vector<float> whiteRGB, blackRGB;
    float cameraGamma{1.f};
    int projectorIndex{};
    double latencyMs{};
    bool keepPixels{};
  };

  static Result compute(Job job);

  //! Writes r as the calibration `path` (CalibrationFiles): its maps, then
  //! name.json.
  static bool saveFile(const QString& path, const Result& r, QString* error = nullptr);
  //! A calibration as saveFile writes it, through the files name.json lists.
  static Result loadFile(const QString& path);
  //! The calibration of projector `index` of a run saved as `path`.
  static QString projectorPath(const QString& path, int index);
};
}
