#pragma once
#include <score_plugin_gfx_export.h>
#include <QMatrix4x4>
#include <QSize>
#include <QVector3D>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ossia
{
struct camera_component;
}

namespace score::gfx
{

// std140 layout; must byte-for-byte match every shader's `uniform camera_t`.
// Packed into ScenePreprocessor's per-camera Camera UBO aux buffer (attached
// to Geometry Out and auto-bound in consuming shaders by name).
struct CameraUBOData
{
  float view[16]{};
  float projection[16]{};
  float viewProjection[16]{};
  float cameraPosition[4]{};
  float renderSize[4]{};
  float params[4]{};
};
static_assert(sizeof(CameraUBOData) == 240, "CameraUBO layout must match shader");

// Byte range the ScenePreprocessor advertises for its `camera` / `camera_prev`
// auxiliary buffers, given the number of cameras it packed into them.
inline constexpr int64_t cameraAuxByteSize(std::size_t cameraCount) noexcept
{
  // The buffer always holds at least one entry: flattenScene publishes a default
  // camera when the scene has none, so a consumer never sees a null binding.
  return (int64_t)((cameraCount < 1 ? 1 : cameraCount) * sizeof(CameraUBOData));
}

inline void writeMat4(float dst[16], const QMatrix4x4& src)
{
  std::memcpy(dst, src.constData(), 16 * sizeof(float));
}

// Reverse-Z perspective projection in OpenGL NDC convention.
//
// Standard OpenGL perspective: view_z ∈ [-far, -near] → NDC z ∈ [-1, +1].
// Reverse-Z (this function):    view_z ∈ [-far, -near] → NDC z ∈ [-1, +1]
//   but INVERTED: near → +1, far → -1.
//
// QRhi's clipSpaceCorrMatrix on Vulkan/Metal/D3D remaps the output NDC z ∈
// [-1, +1] down to the backend-native [0, 1] without further flipping:
// near → 1.0, far → 0.0 in the depth buffer.
//
// This is paired project-wide with a float (D32F) depth attachment, a
// GREATER depth compare and a clear-depth of 0.0. Mixing conventions on a
// single depth buffer produces garbage.
inline void setReverseZPerspective(
    QMatrix4x4& out, float fovYDeg, float aspect, float nearPlane,
    float farPlane)
{
  out.setToIdentity();
  if(nearPlane == farPlane || aspect == 0.f)
    return;

  const float radians = (fovYDeg * 0.5f) * float(M_PI / 180.0);
  const float sine = std::sin(radians);
  if(sine == 0.f)
    return;
  const float cotan = std::cos(radians) / sine;
  const float clip = farPlane - nearPlane;

  out(0, 0) = cotan / aspect;
  out(1, 1) = cotan;
  out(2, 2) = (farPlane + nearPlane) / clip;
  out(2, 3) = (2.f * farPlane * nearPlane) / clip;
  out(3, 2) = -1.f;
  out(3, 3) = 0.f;
}

// Reverse-Z ORTHOGRAPHIC projection, in the same convention as
// setReverseZPerspective above: view_z = -near -> NDC z = +1,
// view_z = -far -> NDC z = -1, paired with a D32F attachment, a GREATER depth
// compare and a clear-depth of 0.0.
//
// halfWidth / halfHeight are the half-extents of the view volume in view space
// — glTF's `xmag` / `ymag`, which is what ossia::camera_component carries.
//
// Derivation of the z row: z_ndc = a*z_view + b with a*(-near) + b = +1 and
// a*(-far) + b = -1  =>  a = 2/(far-near), b = (far+near)/(far-near). w stays 1
// (no perspective divide), which is exactly what makes parallel edges stay
// parallel.
inline void setReverseZOrthographic(
    QMatrix4x4& out, float halfWidth, float halfHeight, float nearPlane,
    float farPlane)
{
  out.setToIdentity();
  if(nearPlane == farPlane || halfWidth == 0.f || halfHeight == 0.f)
    return;

  const float clip = farPlane - nearPlane;
  out(0, 0) = 1.f / halfWidth;
  out(1, 1) = 1.f / halfHeight;
  out(2, 2) = 2.f / clip;
  out(2, 3) = (farPlane + nearPlane) / clip;
  out(3, 2) = 0.f;
  out(3, 3) = 1.f;
}

// The value packCameraUBO writes into CameraUBOData::params[3] (`camera.params.w`
// in a scene shader) so the shader can tell which projection the author asked
// for. 0 is what every camera got before this existed, and it is still what a
// perspective camera gets, so no existing shader changes behaviour.
enum class CameraProjectionMode : int
{
  Perspective = 0,
  Orthographic = 1,
  Fulldome = 2,
};

// Pack a camera_component's view/projection/position into a CameraUBOData.
// `worldTransform` is the camera node's accumulated world matrix (its
// column 3 is the eye position and its inverse is the view matrix).
// `aspectOverride` of <= 0 falls back to `renderSize.width / renderSize.height`.
SCORE_PLUGIN_GFX_EXPORT
void packCameraUBO(
    CameraUBOData& out, const ossia::camera_component& cam,
    const QMatrix4x4& worldTransform, QSize renderSize, float timeSeconds,
    float aspectOverride = -1.f);

}
