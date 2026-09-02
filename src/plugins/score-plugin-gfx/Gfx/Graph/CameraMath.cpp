#include <Gfx/Graph/CameraMath.hpp>

#include <ossia/dataflow/geometry_port.hpp>

namespace score::gfx
{

void packCameraUBO(
    CameraUBOData& out, const ossia::camera_component& cam,
    const QMatrix4x4& worldTransform, QSize renderSize, float timeSeconds,
    float aspectOverride)
{
  const QVector3D eye = worldTransform.column(3).toVector3D();

  QMatrix4x4 view = worldTransform.inverted();

  const float fovYDeg = cam.yfov * (180.f / float(M_PI));
  float aspect = aspectOverride;
  if(aspect <= 0.f)
  {
    aspect = (renderSize.height() > 0)
        ? (float(renderSize.width()) / float(renderSize.height()))
        : (cam.aspect_ratio > 0.f ? cam.aspect_ratio : 1.f);
  }

  // The projection KIND the asset authored. glTF (GltfParser.cpp) and FBX
  // (FbxParser.cpp) both emit orthographic cameras, and camera_component has
  // carried the field since it was introduced -- but this function used to
  // build a perspective frustum unconditionally, so an orthographic or fulldome
  // camera was transported through the whole scene graph and then rendered as a
  // perspective one. The packed UBO is the ONLY channel through which a camera
  // reaches a scene shader, so the information died here.
  QMatrix4x4 proj;
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
      // apply the angular remap. Without the mode the shader had no way to know,
      // which is why fulldome rendered as plain perspective.
      setReverseZPerspective(proj, fovYDeg, aspect, cam.znear, cam.zfar);
      break;
    }
    case ossia::camera_projection::perspective:
    default:
      setReverseZPerspective(proj, fovYDeg, aspect, cam.znear, cam.zfar);
      break;
  }

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
