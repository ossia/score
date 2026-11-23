#pragma once
#include <Gfx/Calibration/Photometric.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
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
    //! Measured an emissive screen, not a projector (name.json's "display").
    bool screen{};
    std::shared_ptr<const score::gfx::photometric::CameraResponse> crf;
    std::vector<float> crfExposures;
    std::optional<score::gfx::ProjectorResponseFile> response;
    //! Per projector pixel: V^-1 (9, row major), identity where unmeasured.
    std::vector<float> compInverse;
    //! Per projector pixel: black / ambient (3), linear camera units.
    std::vector<float> compBlack;
    std::vector<uint8_t> compValid;
    //! Per projector pixel: the largest white s M (1, 1, 1) it can show; 0 where invalid.
    std::vector<float> compWhite;
    //! The target white's chroma folded into M, max component 1.
    std::array<double, 3> compTarget{1., 1., 1.};
    //! Largest white the given fraction of the pixels reaches.
    float compScale{};
    float compGamut{0.85f}, compStrength{1.f}, compGradient{};
    bool compBlackOn{};
    sl::GraySequence seq;
    float phaseValid{};
    float ransac{};
    int intrinsicsMode{};
    bool intrinsicsLocked{};
    bool fromFile{};
    //! From the pattern shown to the camera seeing it, as measured.
    double latencyMs{};
  };

  enum ExtraKind
  {
    RampFrame,
    PrimaryFrame,
    CrfFrame,
    MixFrame,
    GridFrame
  };
  //! A uniform frame after the Gray code.
  struct Extra
  {
    std::array<float, 3> rgb{};
    //! Sent on Camera exposure before the frame; 0 = unchanged.
    float exposure{};
    int kind{};
    //! 0 red, 1 green, 2 blue, 3 grey.
    int channel{};
    //! Kept at full resolution, else every `stride` pixel in x and y.
    bool full{};
    //! Raw camera RGB.
    std::vector<float> rgbCapture;
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
    std::vector<Extra> extras;
    int extraStride{4};
    int mixLevels{};
    std::shared_ptr<const score::gfx::photometric::CameraResponse> crf;
    //! A camera response was recovered or loaded but rejected as implausible.
    bool crfRejected{};
    float compGamut{0.85f}, compStrength{1.f}, compGradient{};
    bool compBlack{};
    double latencyMs{};
    bool keepPixels{};
  };

  static Result compute(Job job);

  //! The frames after the Gray code of a run for projector `index`.
  static std::vector<Extra> makeExtras(
      int responseLevels, bool compensation, const std::vector<float>& exposures,
      int index, int mixLevels = 0);
  //! Camera response from the CrfFrame extras (grey levels at each exposure);
  //! null when there are none or it does not rise with the codes.
  static std::shared_ptr<const score::gfx::photometric::CameraResponse>
  solveCameraResponse(const Job& job, double lambda = 50.);
  static bool plausible(const score::gfx::photometric::CameraResponse& crf);
  static QByteArray encodeCameraResponse(
      const score::gfx::photometric::CameraResponse& crf,
      const std::vector<float>& exposures);
  static std::shared_ptr<const score::gfx::photometric::CameraResponse>
  decodeCameraResponse(const QByteArray& json);
  //! The compensation matrices of r at its scale, strength and black option.
  static score::gfx::CompensationMaps compensationMaps(const Result& r);
  //! Per projector pixel, the white scale applied by compensationMaps.
  static std::vector<float> compensationScale(const Result& r);
  //! The compensations of a run's results brought to one common target.
  static void commonCompensation(std::vector<Result>& results);

  //! Writes r as the calibration `path` (CalibrationFiles): its maps, its
  //! camera response, projector response and compensation, then name.json.
  static bool saveFile(const QString& path, const Result& r, QString* error = nullptr);
  //! A calibration as saveFile writes it, through the files name.json lists.
  static Result loadFile(const QString& path);
  //! The calibration of projector `index` of a run saved as `path`.
  static QString projectorPath(const QString& path, int index);

private:
  static void computePhotometric(
      const Job& job, const std::vector<sl::DecodedPixel>& dec, Result& r,
      std::string& note);
  static void addPhotometricFiles(
      const Result& r, CalibrationWrite& w, QJsonObject& photometric);
  static void loadPhotometricFiles(const QString& base, Result& r, std::string& note);
};
}
