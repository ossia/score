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

  QMatrix4x4 proj;
  setReverseZPerspective(proj, fovYDeg, aspect, cam.znear, cam.zfar);

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
  out.params[3] = 0.f;
}

}
