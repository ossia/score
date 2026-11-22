#include <Gfx/Graph/CameraMath.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace score::gfx
{

bool cameraFrustumTangents(
    const ossia::camera_component& cam, float aspect, float out[4]) noexcept
{
  if(cam.projection != ossia::camera_projection::perspective)
    return false;
  if(cam.frustum_set)
  {
    std::copy_n(cam.frustum, 4, out);
    return true;
  }
  if(!(aspect > 0.f) && cam.aspect_ratio_set)
    aspect = cam.aspect_ratio;
  if(!(aspect > 0.f) || !(cam.yfov > 0.f) || !(cam.yfov < float(M_PI)))
    return false;
  const float ty = std::tan(cam.yfov * 0.5f);
  const float tx = ty * aspect;
  const float cx = 2.f * cam.shift[0] * tx, cy = 2.f * cam.shift[1] * ty;
  out[0] = cx - tx;
  out[1] = cx + tx;
  out[2] = cy - ty;
  out[3] = cy + ty;
  return true;
}

CameraProjectionMode cameraProjection(
    QMatrix4x4& proj, const ossia::camera_component& cam, float aspect)
{
  const float fovYDeg = cam.yfov * (180.f / float(M_PI));

  // The projection KIND the asset authored. glTF (GltfParser.cpp) and FBX
  // (FbxParser.cpp) both emit orthographic cameras, and camera_component
  // carries the field. The packed UBO is the ONLY channel through which a
  // camera reaches a scene shader, so a frustum built unconditionally as
  // perspective here would render an orthographic or fulldome camera as a
  // perspective one.
  auto mode = CameraProjectionMode::Perspective;
  switch(cam.projection)
  {
    case ossia::camera_projection::orthographic:
    {
      mode = CameraProjectionMode::Orthographic;

      // xmag / ymag are the half-extents of the view volume (the glTF
      // definition). An importer that only filled one of them gets the other
      // from the render aspect rather than a degenerate zero.
      float halfH = cam.ymag > 0.f
                        ? cam.ymag
                        : (cam.xmag > 0.f && aspect > 0.f ? cam.xmag / aspect : 1.f);
      float halfW = cam.xmag > 0.f ? cam.xmag : halfH * aspect;
      setReverseZOrthographic(proj, halfW, halfH, cam.znear, cam.zfar);
      break;
    }
    case ossia::camera_projection::fulldome:
    {
      mode = CameraProjectionMode::Fulldome;

      // A dome master is an ANGULAR (fisheye) mapping, not a 4x4: at 180 degrees
      // the perspective frustum is degenerate (tan(90) is infinite), so there is
      // no matrix that expresses it. What this layer owes the renderer is the
      // linear part -- the frustum that culling and the depth buffer work
      // against -- plus the mode, in params.w, that tells the scene shader to
      // apply the angular remap. Without the mode the shader has no way to
      // know, and fulldome renders as plain perspective.
      setReverseZPerspective(proj, fovYDeg, aspect, cam.znear, cam.zfar);
      break;
    }
    case ossia::camera_projection::perspective:
    default:
      if(float t[4]; (cam.frustum_set || cam.shift[0] != 0.f || cam.shift[1] != 0.f)
                     && cameraFrustumTangents(cam, aspect, t))
        setReverseZFrustum(proj, t[0], t[1], t[2], t[3], cam.znear, cam.zfar);
      else
        setReverseZPerspective(proj, fovYDeg, aspect, cam.znear, cam.zfar);
      break;
  }

  if(cam.projection_matrix_set)
  {
    std::memcpy(proj.data(), cam.projection_matrix, 16 * sizeof(float));
    flipProjectionDepth(proj);
  }
  return mode;
}

void packCameraUBO(
    CameraUBOData& out, const ossia::camera_component& cam,
    const QMatrix4x4& worldTransform, QSize renderSize, float timeSeconds,
    float aspectOverride)
{
  const QVector3D eye = worldTransform.column(3).toVector3D();

  QMatrix4x4 view = worldTransform.inverted();

  float aspect = aspectOverride;
  if(aspect <= 0.f && cam.aspect_ratio_set && cam.aspect_ratio > 0.f)
    aspect = cam.aspect_ratio;
  if(aspect <= 0.f)
  {
    aspect = (renderSize.height() > 0)
        ? (float(renderSize.width()) / float(renderSize.height()))
        : (cam.aspect_ratio > 0.f ? cam.aspect_ratio : 1.f);
  }

  QMatrix4x4 proj;
  const auto mode = cameraProjection(proj, cam, aspect);

  QMatrix4x4 vp = proj * view;

  writeMat4(out.view, view);
  writeMat4(out.projection, proj);
  writeMat4(out.viewProjection, vp);
  out.cameraPosition[0] = eye.x();
  out.cameraPosition[1] = eye.y();
  out.cameraPosition[2] = eye.z();
  out.cameraPosition[3] = 0.f;
  out.renderSize[0] = float(renderSize.width());
  out.renderSize[1] = float(renderSize.height());
  out.renderSize[2] = 0.f;
  out.renderSize[3] = 0.f;
  out.params[0] = timeSeconds;
  out.params[1] = cam.znear;
  out.params[2] = cam.zfar;
  out.params[3] = float(int(mode));
}

}
