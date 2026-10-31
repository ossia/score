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

// Index into the scene's camera list of the camera packed at `slot`: the active
// camera first, then the others cyclically in insertion order, so a camera
// array's faces keep their order after whichever of them is active.
inline constexpr std::size_t
cameraPackIndex(std::size_t slot, std::size_t count, int active) noexcept
{
  const std::size_t a = active < 0 ? 0u : std::size_t(active);
  return count == 0 ? 0u : (a + slot) % count;
}

inline void writeMat4(float dst[16], const QMatrix4x4& src)
{
  std::memcpy(dst, src.constData(), 16 * sizeof(float));
}

// Reverse-Z perspective projection in OpenGL NDC convention.
//
// Standard OpenGL perspective: view_z ∈ [-far, -near] → NDC z ∈ [-1, +1].
// Reverse-Z (this function):    view_z ∈ [-far, -near] → NDC z ∈ [-1, +1]
//   but inverted: near → +1, far → -1.
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

// Reverse-Z off-axis perspective, same depth convention as
// setReverseZPerspective. left/right/bottom/top are view-space tangents at
// unit distance (glFrustum's bounds divided by near).
inline void setReverseZFrustum(
    QMatrix4x4& out, float left, float right, float bottom, float top,
    float nearPlane, float farPlane)
{
  out.setToIdentity();
  if(nearPlane == farPlane || right == left || top == bottom)
    return;

  const float clip = farPlane - nearPlane;
  out(0, 0) = 2.f / (right - left);
  out(0, 2) = (right + left) / (right - left);
  out(1, 1) = 2.f / (top - bottom);
  out(1, 2) = (top + bottom) / (top - bottom);
  out(2, 2) = (farPlane + nearPlane) / clip;
  out(2, 3) = (2.f * farPlane * nearPlane) / clip;
  out(3, 2) = -1.f;
  out(3, 3) = 0.f;
}

struct CameraFrustum
{
  float left{-1.f}, right{1.f}, bottom{-1.f}, top{1.f};
};

//! OpenCV intrinsics of a W x H image (pixel centres at integers, v down) to
//! the view-space tangents of the frustum through the image borders.
inline CameraFrustum frustumFromIntrinsics(
    float fx, float fy, float cx, float cy, float width, float height) noexcept
{
  if(fx == 0.f || fy == 0.f)
    return {};
  return CameraFrustum{
      .left = (-0.5f - cx) / fx,
      .right = (width - 0.5f - cx) / fx,
      .bottom = (cy - height + 0.5f) / fy,
      .top = (cy + 0.5f) / fy};
}

//! Vertical FOV, aspect W/H and lens shift (fractions of the image, +y up) to
//! the equivalent frustum tangents.
inline CameraFrustum frustumFromFov(
    float fovYDeg, float aspect, float shiftX, float shiftY) noexcept
{
  const float t = std::tan(fovYDeg * 0.5f * float(M_PI / 180.0));
  const float h = t * aspect;
  return CameraFrustum{
      .left = h * (-1.f + 2.f * shiftX),
      .right = h * (1.f + 2.f * shiftX),
      .bottom = t * (-1.f + 2.f * shiftY),
      .top = t * (1.f + 2.f * shiftY)};
}

//! Turns a standard OpenGL projection (near -> -1) into the reverse-Z one
//! (near -> +1) and back.
inline void flipProjectionDepth(QMatrix4x4& m) noexcept
{
  for(int c = 0; c < 4; c++)
    m(2, c) = -m(2, c);
}

// The value packCameraUBO writes into CameraUBOData::params[3] (`camera.params.w`
// in a scene shader) so the shader can tell which projection the author asked
// for. Perspective is 0, so a shader that ignores the field still reads the
// common case correctly.
enum class CameraProjectionMode : int
{
  Perspective = 0,
  Orthographic = 1,
  Fulldome = 2,
};

// The reverse-Z projection of a camera_component; `aspect` is used only by the
// forms that do not carry their own (yfov, orthographic magnifications).
SCORE_PLUGIN_GFX_EXPORT
CameraProjectionMode cameraProjection(
    QMatrix4x4& out, const ossia::camera_component& cam, float aspect);

// Pack a camera_component's view/projection/position into a CameraUBOData.
// `worldTransform` is the camera node's accumulated world matrix (its
// column 3 is the eye position and its inverse is the view matrix).
// `aspectOverride` of <= 0 falls back to the camera's own aspect when it has
// `aspect_ratio_set`, then to `renderSize.width / renderSize.height`.
SCORE_PLUGIN_GFX_EXPORT
void packCameraUBO(
    CameraUBOData& out, const ossia::camera_component& cam,
    const QMatrix4x4& worldTransform, QSize renderSize, float timeSeconds,
    float aspectOverride = -1.f);

}
