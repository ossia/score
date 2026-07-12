#pragma once
#include <QMatrix4x4>
#include <QSize>
#include <QVector3D>

#include <cmath>
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

// Pack a camera_component's view/projection/position into a CameraUBOData.
// `worldTransform` is the camera node's accumulated world matrix (its
// column 3 is the eye position and its inverse is the view matrix).
// `aspectOverride` of <= 0 falls back to `renderSize.width / renderSize.height`.
void packCameraUBO(
    CameraUBOData& out, const ossia::camera_component& cam,
    const QMatrix4x4& worldTransform, QSize renderSize, float timeSeconds,
    float aspectOverride = -1.f);

}
