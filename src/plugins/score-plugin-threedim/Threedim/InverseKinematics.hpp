#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace Threedim
{

// Two-bone analytical IK solver operating on a scene_spec's skeleton.
//
// Given a 3-joint chain (root → mid → end), a target world-space position,
// and a pole vector (to disambiguate the elbow plane), produces the joint
// rotations that make the end effector reach — or as close as possible to —
// the target. Law-of-cosines closed form, runs in ~50 floating-point ops,
// no iteration.
//
// The solver reads the input skeleton's TRS, finds the named end joint,
// walks two parents up to identify the chain, and emits a scene_spec with
// ONLY the three joints' local rotations modified. The rest of the
// skeleton and the mesh / material data pass through unchanged.
//
// This is the "reach for that door handle" IK — for full articulated
// rigs with >2 bones, spine chains, or pole-axis constraints, chain a
// sequence of these per limb, or write a FABRIK/CCD successor that
// operates on N-joint chains. The interface is intentionally narrow so
// swapping in more sophisticated solvers later doesn't break patches.
//
// Limitations:
//   - no joint-limit / rotation-constraint support yet
//   - no twist decomposition
//   - chain must be a direct parent line in the skeleton; siblings / branches
//     aren't supported
//   - target-unreachable case: extends the chain fully toward the target
//     (the natural "straight-arm stretch" behaviour).
class InverseKinematics
{
public:
  halp_meta(name, "Inverse Kinematics (2-bone)")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "inverse_kinematics")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/inverse-kinematics.html")
  halp_meta(uuid, "6e9f2a4c-1b85-4d3e-a7f6-8c2b4d5e9a0f")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    halp::lineedit<"End joint name", "hand_r"> end_joint;

    halp::xyz_spinboxes_f32<
        "Target",
        halp::range{-10000., 10000., 0.}>
        target;
    halp::xyz_spinboxes_f32<
        "Pole vector",
        halp::range{-10000., 10000., 0.}>
        pole;

    halp::hslider_f32<"Weight", halp::range{0., 1., 1.}> weight;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene Out");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};

  static QVector3D toVec(const float v[3]) { return QVector3D(v[0], v[1], v[2]); }
  static QQuaternion toQuat(const float v[4])
  {
    return QQuaternion(v[3], v[0], v[1], v[2]);
  }
  static void fromQuat(float v[4], const QQuaternion& q)
  {
    v[0] = q.x(); v[1] = q.y(); v[2] = q.z(); v[3] = q.scalar();
  }

  // Compute world-space position of joint `idx` by walking up the parent
  // chain and composing TRS transforms.
  static QVector3D worldJointPos(
      const ossia::skeleton_component& skel, int32_t idx)
  {
    if(idx < 0 || idx >= (int32_t)skel.joints.size())
      return QVector3D();

    // Build a chain from root to idx, then compose forward.
    ossia::small_vector<int32_t, 16> chain;
    for(int32_t i = idx; i >= 0; i = skel.joints[i].parent_index)
      chain.push_back(i);
    std::reverse(chain.begin(), chain.end());

    QMatrix4x4 M;
    for(int32_t i : chain)
    {
      const auto& j = skel.joints[i];
      QMatrix4x4 T;
      T.translate(j.translation[0], j.translation[1], j.translation[2]);
      T.rotate(QQuaternion(
          j.rotation[3], j.rotation[0], j.rotation[1], j.rotation[2]));
      T.scale(j.scale[0], j.scale[1], j.scale[2]);
      M = M * T;
    }
    return M.map(QVector3D());
  }

  // 2-bone IK core: given three world positions + target + pole, compute
  // the rotations (world-space) to apply at the root and mid joints so that
  // end reaches the target. Returns the delta rotations as quaternions.
  struct Solution
  {
    QQuaternion rootDelta;
    QQuaternion midDelta;
  };
  static Solution solve2Bone(
      QVector3D root, QVector3D mid, QVector3D end,
      QVector3D target, QVector3D pole)
  {
    const float eps = 1e-6f;
    QVector3D r2m = mid - root;
    QVector3D m2e = end - mid;
    QVector3D r2e = end - root;
    QVector3D r2t = target - root;

    const float lA   = r2m.length();
    const float lB   = m2e.length();
    const float lTgt = std::min(r2t.length(), lA + lB - eps);
    if(lA < eps || lB < eps || lTgt < eps)
      return {QQuaternion(), QQuaternion()};

    // New elbow interior angle via law of cosines:
    // cos(theta) = (lA² + lB² - lTgt²) / (2 lA lB)
    const float cosNew = std::clamp(
        (lA * lA + lB * lB - lTgt * lTgt) / (2.0f * lA * lB), -1.0f, 1.0f);
    const float thetaNew = std::acos(cosNew);

    // Current elbow interior angle.
    const float cosCur = std::clamp(
        QVector3D::dotProduct(-r2m.normalized(), m2e.normalized()),
        -1.0f, 1.0f);
    const float thetaCur = std::acos(cosCur);

    // Rotation axis for the elbow: perpendicular to the current arm plane,
    // oriented by the pole vector so we pick the "elbow side".
    QVector3D planeNormal = QVector3D::crossProduct(r2m, m2e);
    if(planeNormal.lengthSquared() < eps)
    {
      // Arm is straight → use pole vector's projected perpendicular.
      QVector3D poleDir = (pole - root).normalized();
      planeNormal = QVector3D::crossProduct(r2e.normalized(), poleDir);
      if(planeNormal.lengthSquared() < eps)
        planeNormal = QVector3D(0, 1, 0);
    }
    planeNormal.normalize();

    QQuaternion elbowDelta = QQuaternion::fromAxisAndAngle(
        planeNormal, (thetaCur - thetaNew) * 180.0f / float(M_PI));

    // Rotate the shoulder so the new r2m points toward target minus the
    // elbow contribution.
    QVector3D r2t_n = r2t.normalized();
    QVector3D r2e_n = r2e.normalized();
    QQuaternion rootDelta = QQuaternion::rotationTo(r2e_n, r2t_n);

    return {rootDelta, elbowDelta};
  }

  void operator()()
  {
    const auto& in = inputs.scene_in.scene;
    if(!in.state || !in.state->roots)
    {
      outputs.scene_out.scene.state.reset();
      outputs.scene_out.dirty = 0;
      return;
    }

    // Find the skeleton: first skeleton_component referenced by any mesh.
    const ossia::skeleton_component* srcSkel = nullptr;
    if(in.state->skeletons && !in.state->skeletons->empty())
      srcSkel = (*in.state->skeletons)[0].get();
    if(!srcSkel || srcSkel->joints.empty())
    {
      outputs.scene_out.scene = in; // passthrough
      outputs.scene_out.dirty = 0;
      return;
    }

    const std::string endName = inputs.end_joint.value;
    int32_t endIdx = srcSkel->find_joint(endName);
    if(endIdx < 0 || srcSkel->joints[endIdx].parent_index < 0)
    {
      outputs.scene_out.scene = in;
      outputs.scene_out.dirty = 0;
      return;
    }
    const int32_t midIdx  = srcSkel->joints[endIdx].parent_index;
    if(srcSkel->joints[midIdx].parent_index < 0)
    {
      outputs.scene_out.scene = in;
      outputs.scene_out.dirty = 0;
      return;
    }
    const int32_t rootIdx = srcSkel->joints[midIdx].parent_index;

    // Current world-space joint positions.
    QVector3D wRoot = worldJointPos(*srcSkel, rootIdx);
    QVector3D wMid  = worldJointPos(*srcSkel, midIdx);
    QVector3D wEnd  = worldJointPos(*srcSkel, endIdx);

    QVector3D target(
        inputs.target.value.x, inputs.target.value.y, inputs.target.value.z);
    QVector3D pole(
        inputs.pole.value.x, inputs.pole.value.y, inputs.pole.value.z);

    Solution sol = solve2Bone(wRoot, wMid, wEnd, target, pole);

    // Blend by weight. At weight=0 the output scene is the input unchanged.
    const float w = std::clamp(inputs.weight.value, 0.0f, 1.0f);
    if(w <= 0.0f)
    {
      outputs.scene_out.scene = in;
      outputs.scene_out.dirty = 0;
      return;
    }
    QQuaternion rootDelta = QQuaternion::slerp(QQuaternion(), sol.rootDelta, w);
    QQuaternion midDelta  = QQuaternion::slerp(QQuaternion(), sol.midDelta,  w);

    // Copy the skeleton and mutate the two rotations. Keep other joints
    // untouched so downstream animation / rendering sees a minimal diff.
    auto newSkel = std::make_shared<ossia::skeleton_component>(*srcSkel);

    // These deltas are in world space. Translate to local (parent-relative)
    // rotation by undoing the parent's accumulated rotation.
    auto worldRotOf = [&](int32_t idx) {
      QQuaternion q;
      for(int32_t i = idx; i >= 0; i = srcSkel->joints[i].parent_index)
      {
        QQuaternion local(
            srcSkel->joints[i].rotation[3],
            srcSkel->joints[i].rotation[0],
            srcSkel->joints[i].rotation[1],
            srcSkel->joints[i].rotation[2]);
        q = local * q;
      }
      return q;
    };
    QQuaternion parentRoot = srcSkel->joints[rootIdx].parent_index >= 0
        ? worldRotOf(srcSkel->joints[rootIdx].parent_index)
        : QQuaternion();
    QQuaternion parentMid  = worldRotOf(rootIdx);

    QQuaternion rootLocalNew
        = parentRoot.inverted() * rootDelta * parentRoot
        * toQuat(srcSkel->joints[rootIdx].rotation);
    QQuaternion midLocalNew
        = parentMid.inverted() * midDelta * parentMid
        * toQuat(srcSkel->joints[midIdx].rotation);

    fromQuat(newSkel->joints[rootIdx].rotation, rootLocalNew);
    fromQuat(newSkel->joints[midIdx].rotation,  midLocalNew);
    newSkel->dirty_index++;

    // Build the output scene_state — shallow copy of input, swap the
    // skeletons vector to contain our mutated skeleton.
    if(!m_state || m_state->version != in.state->version - 1)
      m_state = std::make_shared<ossia::scene_state>(*in.state);
    else
      *m_state = *in.state;

    auto skels = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    if(in.state->skeletons)
      *skels = *in.state->skeletons;
    if(skels->empty())
      skels->push_back(newSkel);
    else
      (*skels)[0] = newSkel;
    m_state->skeletons = std::move(skels);
    m_version++;
    m_state->version = m_version;

    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = ossia::scene_port::dirty_transform;
  }
};

}
