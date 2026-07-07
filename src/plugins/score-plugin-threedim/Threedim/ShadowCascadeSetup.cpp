#include "ShadowCascadeSetup.hpp"

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <cmath>

#include <algorithm>
#include <cstring>

namespace Threedim
{

namespace
{

// Compute one cascade's orthographic light view_projection matrix such
// that every corner of the camera-frustum slice between `near` and
// `far` maps inside the unit cube [-1, 1]³ after the light transform.
//
// Steps:
//   1. Build 8 frustum-slice world-space corners from the camera
//      view_proj inverse + near/far clip-space Zs.
//   2. Transform them into light view-space (camera facing -Z along
//      `lightDir`, up arbitrary-but-orthogonal).
//   3. Axis-aligned-bounding-box → light-space ortho projection.
QMatrix4x4 cascadeLightVP(
    const QMatrix4x4& cameraVPInv, float nearZ, float farZ,
    const QVector3D& lightDir)
{
  // Frustum corner coords in NDC. Using OpenGL-ish [-1, 1] — the host
  // clip-space correction matrix handles the Vulkan flip downstream.
  QVector3D corners[8] = {
      QVector3D(-1.f, -1.f, nearZ), QVector3D( 1.f, -1.f, nearZ),
      QVector3D(-1.f,  1.f, nearZ), QVector3D( 1.f,  1.f, nearZ),
      QVector3D(-1.f, -1.f, farZ),  QVector3D( 1.f, -1.f, farZ),
      QVector3D(-1.f,  1.f, farZ),  QVector3D( 1.f,  1.f, farZ)};

  QVector3D world_corners[8];
  QVector3D centroid(0, 0, 0);
  for(int i = 0; i < 8; ++i)
  {
    // Unproject to world.
    QVector4D clip(corners[i], 1.f);
    QVector4D w = cameraVPInv * clip;
    world_corners[i] = w.toVector3D() / w.w();
    centroid += world_corners[i];
  }
  centroid /= 8.f;

  // Light view: looking along lightDir, centered at the slice centroid.
  QVector3D up(0, 1, 0);
  if(std::abs(QVector3D::dotProduct(lightDir.normalized(), up)) > 0.95f)
    up = QVector3D(1, 0, 0);
  QMatrix4x4 lightView;
  lightView.lookAt(centroid - lightDir.normalized() * 1.f, centroid, up);

  // Compute AABB of slice corners in light-view space.
  QVector3D minLS(std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max());
  QVector3D maxLS = -minLS;
  for(int i = 0; i < 8; ++i)
  {
    QVector3D ls = lightView.map(world_corners[i]);
    minLS.setX(std::min(minLS.x(), ls.x()));
    minLS.setY(std::min(minLS.y(), ls.y()));
    minLS.setZ(std::min(minLS.z(), ls.z()));
    maxLS.setX(std::max(maxLS.x(), ls.x()));
    maxLS.setY(std::max(maxLS.y(), ls.y()));
    maxLS.setZ(std::max(maxLS.z(), ls.z()));
  }
  // Expand the depth range toward the light so occluders between the
  // light and the slice (e.g. a tall object above the view frustum) are
  // kept by the near plane and still cast into the slice. In light-view
  // space the eye looks along the light direction, so "toward the
  // light" is +Z (maxLS); padding minLS would only include geometry
  // beyond the slice, which cannot cast shadows into it.
  const float zPad = (maxLS.z() - minLS.z()) * 0.25f + 1.f;
  maxLS.setZ(maxLS.z() + zPad);

  QMatrix4x4 lightProj;
  lightProj.ortho(
      minLS.x(), maxLS.x(), minLS.y(), maxLS.y(),
      -maxLS.z(), -minLS.z());

  // The shadow depth passes run with the project's reverse-Z convention
  // (depth op Greater, clear 0); a standard-Z ortho would keep the
  // farthest surface instead of the nearest occluder. Flip NDC z so
  // near→+1 / far→-1, the same bake ModelDisplay applies to its
  // perspective projection.
  QMatrix4x4 zFlip;
  zFlip(2, 2) = -1.0f;

  return zFlip * lightProj * lightView;
}

// Resolve the first directional light's world direction from the scene
// tree. Recurses through scene_nodes, accumulating parent TRS, and
// matches any light_component whose type == directional — regardless of
// which source node emitted it. Returns false when no directional light
// is found.
bool findDirectionalLight(
    const ossia::scene_node& n, const QMatrix4x4& parentWorld,
    QVector3D& outDir) noexcept
{
  QMatrix4x4 local;
  if(n.children)
  {
    for(const auto& p : *n.children)
    {
      if(auto* xf = ossia::get_if<ossia::scene_transform>(&p))
      {
        local.translate(xf->translation[0], xf->translation[1], xf->translation[2]);
        local.rotate(QQuaternion(
            xf->rotation[3], xf->rotation[0], xf->rotation[1], xf->rotation[2]));
        local.scale(xf->scale[0], xf->scale[1], xf->scale[2]);
        break;
      }
    }
  }
  const QMatrix4x4 world = parentWorld * local;
  if(n.children)
  {
    for(const auto& p : *n.children)
    {
      if(auto* lc = ossia::get_if<ossia::light_component_ptr>(&p))
      {
        if(*lc && (*lc)->type == ossia::light_type::directional)
        {
          // Directional light convention (the Light node encodes the
          // user's direction as a rotation of canonical local -Z via
          // QQuaternion::rotationTo, so local -Z points along the
          // configured direction). World direction is therefore the
          // -Z column of the world matrix.
          QVector3D nZ = world.mapVector(QVector3D(0, 0, -1));
          if(nZ.lengthSquared() > 1e-5f)
          {
            outDir = nZ.normalized();
            return true;
          }
        }
      }
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
        if(*sub && findDirectionalLight(**sub, world, outDir))
          return true;
    }
  }
  return false;
}

// Resolve the active camera's view + projection matrices from the scene
// tree. Walks the same way as findDirectionalLight: per-node TRS
// accumulation into a world matrix, then on hitting a camera_component
// we invert the world matrix to obtain the view. Matching policy:
//   - if `state.active_camera_id` is non-zero, only the scene_node whose
//     id equals it is accepted;
//   - otherwise the first camera encountered wins (matches the "single
//     Camera node is auto-picked" convention from Camera.hpp).
bool findActiveCamera(
    const ossia::scene_node& n, const QMatrix4x4& parentWorld,
    const ossia::scene_state& state, float aspect,
    QMatrix4x4& outView, QMatrix4x4& outProj) noexcept
{
  QMatrix4x4 local;
  if(n.children)
  {
    for(const auto& p : *n.children)
    {
      if(auto* xf = ossia::get_if<ossia::scene_transform>(&p))
      {
        local.translate(xf->translation[0], xf->translation[1], xf->translation[2]);
        local.rotate(QQuaternion(
            xf->rotation[3], xf->rotation[0], xf->rotation[1], xf->rotation[2]));
        local.scale(xf->scale[0], xf->scale[1], xf->scale[2]);
        break;
      }
    }
  }
  const QMatrix4x4 world = parentWorld * local;
  const bool id_filter = state.active_camera_id.value != 0;
  const bool id_matches = !id_filter || n.id == state.active_camera_id;
  if(n.children)
  {
    for(const auto& p : *n.children)
    {
      if(id_matches)
      {
        if(auto* cc = ossia::get_if<ossia::camera_component_ptr>(&p))
        {
          if(*cc)
          {
            const auto& cam = **cc;
            outView = world.inverted();
            outProj = QMatrix4x4{};
            outProj.perspective(
                cam.yfov * 180.f / float(M_PI), aspect, cam.znear, cam.zfar);
            return true;
          }
        }
      }
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
        if(*sub && findActiveCamera(**sub, world, state, aspect, outView, outProj))
          return true;
    }
  }
  return false;
}

} // namespace

void ShadowCascadeSetup::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;

  const int count = std::clamp(inputs.cascade_count.value, 1, 8);
  const float cur_dir[3]{
      inputs.light_direction.value.x, inputs.light_direction.value.y,
      inputs.light_direction.value.z};

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_count = count;
  m_cached_distance = inputs.shadow_distance.value;
  m_cached_lambda = inputs.lambda.value;
  m_cached_near = inputs.camera_near.value;
  m_cached_far = inputs.camera_far.value;
  std::copy(cur_dir, cur_dir + 3, m_cached_dir);

  if(!in_state)
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  // Gather inputs for cascade computation.
  const float nearZ = inputs.camera_near.value;
  const float farZ = std::min(inputs.camera_far.value, inputs.shadow_distance.value);
  const float lambda = std::clamp(inputs.lambda.value, 0.f, 1.f);

  // Scene-derived light direction if the control is left at (0,0,0).
  QVector3D lightDir(cur_dir[0], cur_dir[1], cur_dir[2]);
  if(lightDir.lengthSquared() < 1e-6f)
  {
    lightDir = QVector3D(-0.4f, -0.8f, -0.6f);
    if(in_state->roots)
    {
      for(const auto& r : *in_state->roots)
      {
        QVector3D found;
        if(r && findDirectionalLight(*r, QMatrix4x4{}, found))
        {
          lightDir = found;
          break;
        }
      }
    }
  }
  lightDir.normalize();

  // Find the active camera's view_projection by walking the scene tree
  // the same way findDirectionalLight does. The camera's placement lives
  // on its owning scene_node's scene_transform, so view = inverse(world).
  // Fall back to identity when the scene has no camera (the cascades
  // will be approximate but the node stays safe to wire in early).
  //
  // Aspect is unknown at this stage (ScenePreprocessor is the canonical
  // source of the render-target aspect); 16:9 is a reasonable default
  // and the cascade fit is approximate anyway.
  QMatrix4x4 cameraVP;
  QMatrix4x4 cameraProj;
  const float aspect = 16.f / 9.f;
  if(in_state->roots)
  {
    QMatrix4x4 view, proj;
    for(const auto& r : *in_state->roots)
    {
      if(r && findActiveCamera(*r, QMatrix4x4{}, *in_state, aspect, view, proj))
      {
        cameraVP = proj * view;
        cameraProj = proj;
        break;
      }
    }
  }

  const QMatrix4x4 cameraVPInv = cameraVP.inverted();

  // Practical split scheme (Engel/Tabellion).
  ossia::shadow_cascades_info info{};
  info.cascade_count = uint32_t(count);
  info.shadow_distance = inputs.shadow_distance.value;
  info.light_direction[0] = lightDir.x();
  info.light_direction[1] = lightDir.y();
  info.light_direction[2] = lightDir.z();

  info.split_view_depths[0] = nearZ;
  for(int i = 1; i < count; ++i)
  {
    const float p = float(i) / float(count);
    const float logSplit = nearZ * std::pow(farZ / nearZ, p);
    const float uniSplit = nearZ + (farZ - nearZ) * p;
    info.split_view_depths[i] = lambda * logSplit + (1.f - lambda) * uniSplit;
  }
  info.split_view_depths[count] = farZ;

  // NDC-Z range for each cascade slice. glClipSpace uses [-1, 1]; Vulkan
  // uses [0, 1] after clipSpaceCorr — here we work in camera clip-space
  // pre-correction, so [-1, 1] is correct.
  for(int i = 0; i < count; ++i)
  {
    // Convert view-space Z to NDC Z. (0, 0, -d) is a VIEW-space point,
    // so it goes through the projection alone — pushing it through the
    // full view-projection would treat it as world-space and fit the
    // cascade to the wrong depth range for any non-identity camera.
    QVector4D p0 = cameraProj * QVector4D(0, 0, -info.split_view_depths[i], 1);
    QVector4D p1 = cameraProj * QVector4D(0, 0, -info.split_view_depths[i + 1], 1);
    const float ndc0 = p0.w() != 0.f ? p0.z() / p0.w() : -1.f;
    const float ndc1 = p1.w() != 0.f ? p1.z() / p1.w() : 1.f;
    QMatrix4x4 m = cascadeLightVP(cameraVPInv, ndc0, ndc1, lightDir);
    std::memcpy(info.light_view_proj[i], m.constData(), sizeof(float) * 16);
  }

  // Clone scene_state with the new cascades info.
  auto state = std::make_shared<ossia::scene_state>(*in_state);
  state->shadow_cascades = info;
  state->version = ++m_version_counter;
  state->dirty_index = m_version_counter;

  m_cached_out = state;
  m_pending_dirty = 0xFF;
}

void ShadowCascadeSetup::operator()()
{
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const bool upstream_changed
      = m_cached_in_state != in_state || m_cached_in_version != in_version;
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
