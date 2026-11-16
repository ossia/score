#pragma once
#include <Gfx/Calibration/Photometric.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Threedim/StructuredLight.hpp>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{
//! What Structured Light Calibration computes, independent of how the patterns
//! are shown and the camera frames reach it: the process and the calibration
//! tool both run a StructuredLightSession over these.
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

  //! What a run (or a loaded file) produced.
  struct Result
  {
    bool ok{};
    std::string status;
    int width{}, height{};
    std::vector<float> xyz;
    int camWidth{}, camHeight{};
    std::vector<uint8_t> decoded;
    std::array<float, 4> intrinsics{};
    std::array<float, 16> pose{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float rms{};
    float coverage{};
    int mode{};
    //! n.xyz, d with n . p + d = 0, as ParametricSurface takes them.
    std::vector<std::array<float, 4>> planes;
    //! Per camera pixel.
    std::vector<sl::DecodedPixel> pixels;
    //! RGBA per projector pixel, w = valid; empty without RGB frames.
    std::vector<float> lum, black;
    float cameraGamma{1.f};
    int projectorIndex{};
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
    std::vector<float> rgbCapture;
  };

  struct Job
  {
    std::shared_ptr<const std::atomic_bool> cancel;
    sl::Decoder decoder;
    sl::GraySequence seq;
    std::vector<float> positions;
    std::vector<int> positionCount;
    std::vector<float> zMin, zMax;
    int positionFrames{};
    std::array<float, 16> worldFromCamera{};
    std::array<float, 4> cameraIntrinsics{};
    int mode{};
    float minSignal{}, depthJump{}, ransac{}, planeThreshold{0.02f};
    float phaseMinModulation{0.1f};
    float whiteMax{1.f};
    int intrinsicsMode{};
    bool lockK{};
    std::array<float, 4> lockedK{};
    std::string kNote;
    int minBits{}, holeRadius{}, surface{}, maxPlanes{2};
    //! RGB per camera pixel of the white and black frames.
    std::vector<float> whiteRGB, blackRGB;
    float cameraGamma{1.f};
    int projectorIndex{};
    std::vector<Extra> extras;
    int extraStride{4};
    int mixLevels{};
    std::shared_ptr<const score::gfx::photometric::CameraResponse> crf;
    //! A camera response was recovered but rejected as implausible.
    bool crfRejected{};
    float compGamut{0.85f}, compStrength{1.f}, compGradient{};
    bool compBlack{};
  };

  //! The frames after the Gray code of a run for projector `index`.
  static std::vector<Extra> makeExtras(
      int responseLevels, bool compensation, const std::vector<float>& exposures,
      int index, int mixLevels = 0);
  //! Camera response from the CrfFrame extras (grey levels at each exposure).
  static std::shared_ptr<score::gfx::photometric::CameraResponse>
  solveCameraResponse(const Job& job, double lambda = 50.);
  static bool saveCameraResponse(
      const std::string& path, const score::gfx::photometric::CameraResponse& crf,
      const std::vector<float>& exposures);
  static std::shared_ptr<score::gfx::photometric::CameraResponse>
  loadCameraResponse(const std::string& path);
  //! The compensation matrices of r at its scale, strength and black option.
  static score::gfx::CompensationMaps compensationMaps(const Result& r);
  //! Per projector pixel, the white scale applied by compensationMaps.
  static std::vector<float> compensationScale(const Result& r);
  //! Runs compute on every job, the camera response solved once from the
  //! first, the compensation brought to one common target.
  static std::vector<Result> computeAll(std::vector<Job> jobs);
  //! "x y w h;..." -> one normalised rect per projector; side by side when empty.
  static std::vector<std::array<float, 4>> canvasRects(const std::string& layout, int count);

  static Result compute(Job job);
  static bool saveFile(const std::string& path, const Result& r);
  //! `name` for one projector, `name_pN` for projector N > 0 of a multi-projector run.
  static std::string projectorPath(const std::string& path, int index, int count);
  static Result loadFile(const std::string& path);
};
}
