// =============================================================================
// P2-11 + P2-12 -- two "this is an approximation, and the code says so"
// contracts from SPEC-SCENE-RENDER-TESTS.md section 3.3.
//
// READ THIS FIRST -- ONE OF THE TWO ROWS IS STALE.
//
//   P2-11 (`area lights are point-light approximations`) is REAL and is pinned
//   RED with Catch2's `[!shouldfail]`, as the spec asks.
//
//   P2-12 (`SceneFilterNode mode 2 is not mode 1`) is NOT. The contract the
//   spec row asks for -- "mode 2 differs from mode 1" -- is ALREADY SATISFIED
//   by the shipped code, and it is the shipped DOC-COMMENT that is wrong. It
//   is therefore written as a PLAIN GREEN test, per the rule that a pin must
//   never be manufactured for a behaviour that is already correct. Full
//   evidence in the P2-12 section below. A separate, clearly-labelled
//   RE-SCOPED pin covers the part of that same admission which IS still open
//   (mode 2 is unconfigurable: the node has no Name port at all).
//
// Both pins assert the RIGHT answer, never today's wrong one: on the day the
// product is fixed each pinned entry flips from "failed as expected" to an
// UNEXPECTED PASS, which Catch2 reports as a failure of that entry
// (catch_run_context.cpp:255-259: `expectedToFail() && testCases.passed > 0`
// -> the case is re-counted as failed). That is the signal to delete the tag.
// The untagged cases in this file must be GREEN at all times.
//
// GPU-LESS, on purpose. No QRhi, no QWindow, no display, no document, no file
// I/O except the one source-text read described below. Per section 3.0
// ("Null-RHI is for *decision-logic* assertions only -- which rung was chosen,
// which format refused, what a gate returned") both questions here are
// decision-logic, not pixels: "what code does the encoder emit for this mode"
// and "which nodes does the filter keep". Pixels would answer neither more
// reliably nor more cheaply, and for P2-12 the answer is a scene tree, which
// has no pixel form at all.
//
// -----------------------------------------------------------------------------
// WHY A NEW FILE AND NOT tests/threedim/KnownDefects.cpp
//
// KnownDefects.cpp is the tree's existing home for expected-red contracts and
// was the first candidate. It is not the right home for these, for a reason of
// mechanism rather than taste:
//
//   * KnownDefects.cpp pins at TARGET granularity, not case granularity. It
//     carries no `[!shouldfail]` tag anywhere; its defect cases fail for real,
//     so the whole ctest entry `test_threedim_known_defects` is RED
//     (tests/threedim/CMakeLists.txt, the "EXPECTED RED" block: "Isolated so
//     an open defect does not mask the green suites, and so the entry flips to
//     PASS on the day it is fixed"). The untagged discrimination controls this
//     work requires would be unobservable inside a target that is red as a
//     whole -- and a control nobody can see be green is not a control.
//   * These cases need compile inputs that target does not have:
//     Threedim/Light.cpp compiled in (Light::rebuild() is out-of-line) and
//     THREEDIM_SRC_DIR. test_threedim_known_defects defines only GFX_SRC_DIR
//     and compiles no engine TU.
//
// So: a new file, a new target, `[!shouldfail]` at CASE granularity, and the
// target as a whole stays GREEN -- Catch2 counts a failing assertion inside an
// okToFail case as `failedButOk` (catch_run_context.cpp:286-287) and exits 0,
// and a SKIP inside one is counted as skipped rather than as an unexpected
// pass (catch_totals.cpp:52-62, `testCases.passed` stays 0). A new file was
// required by the task regardless; this is why the convention also fits.
//
// =============================================================================
// P2-11 -- `area lights are point-light approximations`. EXPECT RED.
// =============================================================================
//
// THE ADMISSION, quoted verbatim.
//
//   src/plugins/score-plugin-threedim/Threedim/Light.hpp:37-42, the tail of
//   the class doc-comment (the spec row cites :41-42, its last two lines):
//
//     // ScenePreprocessor packs it into the scene-wide `scene_lights` SSBO via
//     // packLight(). Current consumer shaders (`classic_pbr_*.frag`) only
//     // sample the common fields (position/direction/color/intensity/range +
//     // spot cone angles) -- area-light shapes pass through correctly but
//     // are rendered as point-light approximations until shaders add the
//     // Rect/Disk/Sphere sampling math.
//
//   src/plugins/score-plugin-threedim/Threedim/Light.cpp:137-154, the
//   render-thread encoder itself (the spec row cites :141-154, which is
//   exactly the function; :137-140 is its comment):
//
//     // Mode -> raw type encoding used by RawLightData::local_direction.w and
//     // LightGPU::position_type.w. Area / dome modes collapse onto punctual
//     // analogues for the raw arena (directional for dome, point for rect /
//     // disk / sphere) -- area-light shading is a shader-side extension.
//     inline float toRawLightType(Light::Mode m) noexcept
//     {
//       switch(m)
//       {
//         case Light::Directional: return 0.f;
//         case Light::Point:       return 1.f;
//         case Light::Spot:        return 2.f;
//         case Light::Rect:
//         case Light::Disk:
//         case Light::Sphere:      return 1.f;
//         case Light::Dome:        return 0.f;
//       }
//       return 1.f;
//     }
//
//   and the record it feeds, src/plugins/score-plugin-gfx/Gfx/Graph/
//   SceneGPUState.hpp:311-320, inside `struct RawLightData`:
//
//     float local_direction[4]{0.f, 0.f, -1.f, 0.f}; // xyz = dir (local),
//                                                     // w = type enum:
//                                                     //   0 = directional
//                                                     //   1 = point
//                                                     //   2 = spot
//                                                     // (area / dome modes
//                                                     // collapse to point /
//                                                     // directional; ...
//
// THE CORRECT BEHAVIOUR being asserted. A Light in mode Rect (or Disk, or
// Sphere) must not compute to the same shader-facing light as a Light in mode
// Point sitting at its centre; a Light in mode Dome must not compute to the
// same shader-facing light as a Light in mode Directional. Concretely: the
// per-light type code the render thread writes into
// RawLightData::local_direction.w must be INJECTIVE over Light::Mode
// (Light.hpp:55-64, seven enumerators -> seven codes), because that word is
// the only thing downstream shading branches on.
//
// Injectivity is NECESSARY, not sufficient: a real fix must also carry the
// extent, and RawLightData has nowhere to put it. Its 64 bytes are fully
// committed -- color[4] + local_direction[4] + range_cone[4] + shadow_enabled
// + decay_mode + transform_slot + normal_bias = 16+16+16+4+4+4+4 = 64 -- and
// SceneGPUState.hpp:345 static_asserts against growth. The pin is scoped to
// the half that is decidable here and says so; the record-widening half is
// noted for whoever does the fix.
//
// TWO ORACLES: one already-green, one pinned.
//
//   (1) PRODUCER LEVEL -- value, GPU-less, ALREADY CORRECT, therefore GREEN
//       and UNTAGGED. Threedim::Light::rebuild() (Light.cpp:42-124) is pure
//       CPU and is driven directly. It maps Rect/Disk/Sphere/Dome onto
//       DISTINCT ossia::light_type values (Light.cpp:14-27) and copies
//       width/height/radius onto the emitted ossia::light_component
//       (Light.cpp:70-72; fields at 3rdparty/libossia/src/ossia/dataflow/
//       geometry_port.hpp:852-855). So the scene_spec a Light publishes DOES
//       distinguish a 4x2 rect light from a point light at the same place.
//       This half of the spec row is stale and is recorded here as a plain
//       green regression guard -- and doubles as the required discrimination
//       control for the field comparator (identical configs compare equal, one
//       moved slider compares unequal).
//
//   (2) ENCODER LEVEL -- where the collapse actually lives, and PINNED RED.
//       The encoder is `toRawLightType`, an `inline` function in an ANONYMOUS
//       namespace (Light.cpp:135-160) reachable only from Light::update()
//       (Light.cpp:214).
//
// WHY THE ENCODER PIN READS SOURCE TEXT RATHER THAN CALLING THE ENCODER.
// This is a deliberate, documented compromise -- the weakest thing in this
// file, and the reader should know exactly how weak.
//
//   Light::update() cannot be driven without a real QRhi. Verified, three
//   independent walls:
//     a. Light.cpp:198-199 returns immediately unless `raw_light_slot.valid()`.
//     b. A valid slot only comes from GpuResourceRegistry::allocate(), which
//        bails with a qWarning unless the arena already owns a QRhiBuffer --
//        GpuResourceRegistry.cpp:586-591, "arena ... is not initialised" --
//        and that buffer is created only by GpuResourceRegistry::init(QRhi&,
//        QRhiResourceUpdateBatch&) (GpuResourceRegistry.hpp:115).
//     c. The write itself is GpuResourceRegistry::updateSlot(), which drops
//        the bytes when the arena buffer is null (GpuResourceRegistry.cpp:
//        660-661) and otherwise hands them to a QRhiResourceUpdateBatch, an
//        opaque Qt type with no public constructor. No seam returns the
//        emitted RawLightData to a caller.
//     Nor can a RenderList be faked: its only constructor is
//     `RenderList(OutputNode&, const std::shared_ptr<RenderState>&)`
//     (RenderList.hpp:37) and it acquires the registry from a live output
//     (RenderList.cpp:192-195). The CameraRelease.cpp inert-reference seam
//     does not apply -- there the callee provably never dereferences its
//     RenderList&; here it must.
//
//   So the shipped encoder is read out of the shipped Light.cpp and its switch
//   parsed back into the pure function `Mode -> emitted code` that it is. The
//   comparison run on that function is a genuine difference oracle: the SAME
//   comparator is applied by an untagged control to pairs that must differ
//   (Point vs Directional, Spot vs Point) and to a pair that must agree (Rect
//   vs Disk), so the oracle is shown able to say both "different" and "same"
//   before the pin uses it. What it is NOT is a value-level observation of a
//   running encoder. This is the class of guard the tree already blesses for
//   unreachable constant-text defects -- tests/threedim/CMakeLists.txt: "their
//   honest GPU-free guard reads the SHIPPED engine source and asserts the
//   corrected text (THREEDIM_SRC_DIR)" -- and is what KnownDefects.cpp's
//   issue-#163 case does, for exactly this reason. If a buffer-readback
//   fixture ever lands (spec section 3.4 item 1), replace the parse with
//   init()+update() on a Null-RHI RenderList and read the 64 bytes back; the
//   assertions transfer unchanged.
//
// CONTEXT WORTH HAVING (measured, not assumed):
//   * `grep -rn 'rect_area\|disk_area\|sphere_area' src/` -> FIVE hits, all
//     PRODUCERS: Light.cpp:21,22,23 and FbxParser.cpp:674-675. Nothing in
//     src/ ever READS an area light type. The approximation is total.
//   * score::gfx::flattenScene keeps NOTHING about a light except its arena
//     slot index -- SceneGPUState.cpp:528-540 pushes only
//     `(*light)->raw_slot.internal_index` into FlatScene::lightArenaSlots
//     (SceneGPUState.hpp:492-495). RawLightData is not merely the main channel
//     to the shader, it is the ONLY one, which is what makes this encoder the
//     whole ballgame and what makes a scene-level difference oracle useless
//     for the pinned half.
//
// =============================================================================
// P2-12 -- `SceneFilterNode mode 2 is not mode 1`. **ALREADY CORRECT -> GREEN.**
// =============================================================================
//
// THE ADMISSION the spec row rests on, quoted verbatim from
// src/plugins/score-plugin-gfx/Gfx/Graph/SceneFilterNode.hpp:17-22 (the row
// cites :20-22, its last three lines):
//
//    *   - Port 1: Mode (Types::Int):
//    *        0 = pass-through (no filtering)
//    *        1 = keep only scene_nodes with visible == true
//    *        2 = keep only subtrees whose node name contains the substring set
//    *            in the "Name" control (future-wired; string port missing in the
//    *            renderer for now, so behaves like mode 1 until wired)
//
// THAT LAST CLAUSE IS FALSE. The mode is consulted in exactly two places in
// the whole unit, and both are literal equalities against a single value:
//
//   SceneFilterNode.cpp:45-47, inside SceneFilterVisitor::rewrite_node --
//     // Mode 1: drop invisible subtrees outright.
//     if(mode == 1 && !src->visible)
//       return nullptr;
//
//   SceneFilterNode.cpp:121-123, inside SceneFilterVisitor::rewrite --
//     // Mode 0: pass-through, no copy needed.
//     if(mode == 0)
//       return in;
//
// With mode == 2 the visibility test at :46 does not fire, so mode 2 keeps
// invisible subtrees and mode 1 does not. On any scene containing an invisible
// node the two produce DIFFERENT trees. Mode 2 does not behave like mode 1; it
// behaves like mode 0 -- keep everything -- differing from mode 0 only in
// making a fresh copy and bumping version/dirty_index (SceneFilterNode.cpp:
// 126-141). So the spec row's contract, "mode 2 differs from mode 1", holds
// TODAY and must not be pinned red.
//
// It is written below as a plain green difference-oracle test, because a
// contract that holds by accident of a `== 1` literal is still worth nailing
// down: the single most likely "fix" for the doc-comment is to change :46 to
// `mode != 0`, which would silently make mode 2 into mode 1 for real.
//
// WHAT IS STILL OPEN, and the RE-SCOPED PIN. The other half of the same
// admission -- "string port missing in the renderer for now" -- is true and
// checkable. SceneFilterNode's constructor creates exactly two input ports:
//
//   SceneFilterNode.cpp:219-227
//     SceneFilterNode::SceneFilterNode()
//     {
//       input.push_back(new Port{this, {}, Types::Scene, {}});
//       {
//         auto* data = new int{0};
//         input.push_back(new Port{this, data, Types::Int, {}});
//       }
//       output.push_back(new Port{this, {}, Types::Scene, {}});
//     }
//
// -- Scene and Mode, no Name. Ports are the only input channel a
// score::gfx::Node has (Node.hpp:103, `std::vector<Port*> input`, fed by
// Node::process(Message&&)), so mode 2 is not merely unwired, it is
// UNCONFIGURABLE: no substring can be delivered to it by any route. That is
// the live defect, and it is what the second `[!shouldfail]` pin asserts --
// that a Name input exists. Whoever fixes it will also need a string kind in
// `score::gfx::Types`, which today runs Empty/Int/Float/Vec2/Vec3/Vec4/Image/
// Audio/Camera/Geometry/Buffer/Scene and has none (Uniforms.hpp:10-24); which
// type the Name port ends up with is UNVERIFIED and the pin deliberately does
// not constrain it -- it only requires that a third input port exists.
//
// ORACLE for both P2-12 cases: value-level, GPU-less, on the FILTERED SCENE
// ITSELF -- the real product visitor, never a re-implementation. The whole
// renderer is driven:
//   SceneFilterNode::createRenderer (SceneFilterNode.cpp:245)
//     -> RenderedSceneFilterNode::init (:163)
//     -> RenderedSceneFilterNode::update (:173), which runs
//        SceneFilterVisitor::rewrite (:115-142), the unit under test
//     -> RenderedSceneFilterNode::runInitialPasses (:194-211), the ONLY exit
//        for the private m_outputScene: it calls
//        `process(port_idx, m_outputScene, edge.source)` (:210) on the
//        downstream renderer.
// A stub NodeRenderer is registered as that downstream and the delivered scene
// is read back out of the base class's public m_portScenes (stored there by
// NodeRenderer.cpp:590-596; member at NodeRenderer.hpp:207).
//
// Every RenderList& / QRhiCommandBuffer& / QRhiResourceUpdateBatch& on that
// path is an INERT REFERENCE -- the tests/threedim/CameraRelease.cpp seam, and
// here provable by inspection rather than by argument, because the parameters
// are UNNAMED in the product's own signatures:
//     SceneFilterNode.cpp:163  init(RenderList&, QRhiResourceUpdateBatch&)
//     SceneFilterNode.cpp:173  update(RenderList&, QRhiResourceUpdateBatch&, Edge*)
//     SceneFilterNode.cpp:245  createRenderer(RenderList&) const noexcept
// -- so they cannot be touched; and in runInitialPasses (:194-211) `renderer`
// is used only as `&renderer`, a map key into Node::renderedNodes
// (Node.hpp:114). Nothing is dereferenced. No QRhi is constructed anywhere in
// this file.
//
// THE SCENE, and why it makes the oracle substring-agnostic:
//
//     S = two sibling ROOT nodes with the SAME name "obj",
//         one with visible == false, one with visible == true.
//
// Any name-based predicate is blind to `visible`, so a correct mode 2 must
// return the same verdict for both roots: it keeps 2 (any substring the two
// share, the empty one included) or keeps 0. Mode 1 is defined by `visible`
// and keeps exactly 1. So mode2(S) != mode1(S) is required of EVERY correct
// implementation, for EVERY substring -- which is what lets these cases assert
// something real about a control that does not exist yet.
//
// =============================================================================
// REGISTRATION (append inside tests/threedim/CMakeLists.txt).
// ctest target name: `test_threedim_scene_approximation_pins`.
//
//     # P2-11 (expected-red) + P2-12 (green; its spec row is stale, see the
//     # file header) plus their untagged discrimination controls. P2-11
//     # compiles Light.cpp in (rebuild() is out-of-line) and reads the shipped
//     # Light.cpp text for the render-thread type encoder, which is `inline` in
//     # an anonymous namespace and needs a live QRhi registry to reach -- hence
//     # THREEDIM_SRC_DIR, the ShaderStrings.cpp pattern. P2-12 drives
//     # score::gfx::SceneFilterNode's renderer end to end (createRenderer ->
//     # update -> runInitialPasses) with inert RenderList& / QRhiCommandBuffer&
//     # references, the CameraRelease.cpp seam; SceneFilterNode is
//     # SCORE_PLUGIN_GFX_EXPORT so score_plugin_gfx is LINKED, never
//     # recompiled. Qt Gui for QQuaternion (Light.cpp) and the QMatrix4x4 in
//     # TinyObj.hpp. Pure CPU: no QRhi, no display, no document.
//     score_add_test(test_threedim_scene_approximation_pins
//       SOURCES SceneApproximationPins.cpp
//         "${THREEDIM_DIR}/Light.cpp"
//       PLUGINS score_plugin_gfx
//       LIBS ${QT_PREFIX}::Gui)
//     threedim_test_includes(test_threedim_scene_approximation_pins)
//     target_compile_definitions(test_threedim_scene_approximation_pins PRIVATE
//       THREEDIM_SRC_DIR="${THREEDIM_DIR}")
//
// =============================================================================
// NEGATIVE CONTROLS (product-side, do NOT commit any of them).
//
// The spec gives "n/a" for both rows. For an expected-red pin the useful
// control is the reverse of the usual one: the local product edit that makes
// the PIN PASS, turning the ctest entry into an UNEXPECTED PASS. Both pins get
// one, and each is paired with a forward control that reddens a NAMED subset
// of the untagged cases -- a control that only ever moves one entry proves
// half a thing.
//
// P2-11, MAKE-THE-PIN-PASS (reverse control).
//   src/plugins/score-plugin-threedim/Threedim/Light.cpp:148-151, replace
//
//       case Light::Rect:
//       case Light::Disk:
//       case Light::Sphere:      return 1.f;
//       case Light::Dome:        return 0.f;
//
//   with
//
//       case Light::Rect:        return 3.f;
//       case Light::Disk:        return 4.f;
//       case Light::Sphere:      return 5.f;
//       case Light::Dome:        return 6.f;
//
//   MUST FLIP: "DEFECT P2-11 ..." becomes an unexpected pass (entry red).
//   MUST ALSO REDDEN, expected and correct: the single `CHECK_FALSE(
//   encoder_separates(code, "Rect", "Disk"))` in "P2-11 control: the shipped
//   encoder parse can say BOTH same and different" -- that CHECK is a property
//   of the PARSER, not a contract on the product; the case body says so.
//   MUST STAY GREEN: everything else, including the producer-level control.
//   NOTE this edit is a control, not a fix: RawLightData still has no field
//   for the extent, so area lights would still shade wrong.
//
// P2-11, REDDEN-A-NAMED-SUBSET (forward controls on the untagged half).
//   (i) Light.cpp:147, `case Light::Spot: return 2.f;` -> `return 1.f;`
//       MUST REDDEN: exactly two CHECKs, the Spot ones, in "P2-11 control: the
//       shipped encoder parse can say BOTH same and different".
//       MUST STAY GREEN: the producer-level control, all of P2-12. Pin red.
//   (ii) Light.cpp:70-71, delete
//           lc->width = inputs.width.value;
//           lc->height = inputs.height.value;
//        MUST REDDEN: exactly the two extent CHECKs (`lr->width`, `lr->height`)
//        in "P2-11 (producer, GREEN): ...". The light_type CHECKs in that same
//        case stay green (Light.cpp:14-27 untouched), as does the whole-record
//        `CHECK_FALSE(shading_of(*lr) == shading_of(*lp))` (the types still
//        differ). That split is the point: producer and encoder are
//        independently anchored. Pin red.
//
// P2-12, MAKE-THE-RE-SCOPED-PIN-PASS (reverse control).
//   src/plugins/score-plugin-gfx/Gfx/Graph/SceneFilterNode.cpp:224, after the
//   Mode port, add a third input:
//
//       input.push_back(new Port{this, new std::string{}, Types::Int, {}});
//
//   (any type; the pin constrains only existence -- see the header).
//   MUST FLIP: "DEFECT P2-12 (re-scoped) ..." becomes an unexpected pass.
//   MUST STAY GREEN: every other case in this file, P2-11 included -- nothing
//   else reads input.size().
//
// P2-12, REDDEN-A-NAMED-SUBSET (forward control on the green half).
//   SceneFilterNode.cpp:46, `if(mode == 1 && !src->visible)` ->
//   `if(mode != 0 && !src->visible)`, i.e. actually make mode 2 behave like
//   mode 1, which is what the stale doc-comment claims.
//   MUST REDDEN: exactly the three CHECKs in "P2-12 (GREEN): SceneFilterNode
//   mode 2 already is not mode 1" -- `m2 != m1`, `m2.size() != 1` and the
//   drift-watch `m2 == m0`.
//   MUST STAY GREEN: "P2-12 control: the harness discriminates ..." in full
//   (mode 0 vs mode 1 is untouched) and all of P2-11. The re-scoped pin stays
//   RED (still no Name port). This is the control that matters most for P2-12:
//   it shows the green case is anchored on the mode1-vs-mode2 relation and
//   would catch the exact regression the doc-comment invites.
//
//   Second forward control, for the harness itself:
//   SceneFilterNode.cpp:46 -> `if(false && !src->visible)`.
//   MUST REDDEN: exactly the two mode-1 assertions in "P2-12 control: the
//   harness discriminates ..." (mode 1 stops dropping the invisible root, so
//   mode0 == mode1). MUST STAY GREEN: all of P2-11.
//
// =============================================================================
// STATUS. Written against the sources cited above; every file:line was read at
// authoring time on the c/planf2 worktree ~/ossia/wt/score-interop. NOT
// COMPILED AND NOT RUN in this session (the task forbade building), so
// "the two pins are red today", "the untagged cases are green today" and
// "P2-12's difference oracle is already satisfied" are derived from the code,
// not measured -- UNVERIFIED. Every negative-control outcome predicted above
// is likewise UNVERIFIED. Build the target, confirm the two `[!shouldfail]`
// entries report "failed as expected" and everything else passes, and run the
// controls before recording these rows in the ledger.
// =============================================================================

#include <Threedim/Light.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/SceneFilterNode.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/variant.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{

// =============================================================================
// P2-11 helpers
// =============================================================================

//! Build one Threedim::Light in a given mode with a fixed, explicit control
//! set, tick it, and hand back the single ossia::light_component it published.
//! Everything except `mode` (and whatever a caller deliberately overrides) is
//! held identical across probes, so any difference the comparator reports is
//! attributable to the mode.
struct LightProbe
{
  Threedim::Light node;

  explicit LightProbe(Threedim::Light::Mode mode)
  {
    node.inputs.mode.value = mode;
    node.inputs.intensity.value = 7.5f;
    node.inputs.range.value = 12.f;
    node.inputs.decay.value = Threedim::Light::DecayQuadratic;
    node.inputs.inner_cone.value = 10.f;
    node.inputs.outer_cone.value = 40.f;
    // The extent under test: a 4 x 2 rect, a radius-3 disk / sphere.
    node.inputs.width.value = 4.f;
    node.inputs.height.value = 2.f;
    node.inputs.radius.value = 3.f;
    node.inputs.cast_shadow.value = true;
    node.inputs.shadow_bias.value = 0.002f;
    node.inputs.shadow_normal_bias.value = 0.02f;
    // Every probe sits at the SAME place: "a point light at its centre".
    node.inputs.position.value.x = 1.f;
    node.inputs.position.value.y = 2.f;
    node.inputs.position.value.z = 3.f;
    node.inputs.rotation.value.x = 0.f;
    node.inputs.rotation.value.y = 0.f;
    node.inputs.rotation.value.z = 0.f;
  }

  //! Tick the producer -- Light::rebuild() (Light.cpp:42-124) then
  //! Light::operator()() (Light.cpp:126-133) -- and return the emitted
  //! light_component. Pure CPU; no arena slot is ever valid here, so no
  //! render-thread code is reachable.
  const ossia::light_component* emit()
  {
    node.rebuild();
    node();
    const auto& st = node.outputs.scene_out.scene.state;
    if(!st || !st->roots || st->roots->size() != 1)
      return nullptr;
    const auto& root = (*st->roots)[0];
    if(!root || !root->children)
      return nullptr;
    for(const auto& child : *root->children)
      if(auto* lc = ossia::get_if<ossia::light_component_ptr>(&child))
        return lc->get();
    return nullptr;
  }
};

//! The SHADING-RELEVANT fields of a light_component: everything a renderer
//! could legitimately branch on, and nothing else. Identity (`stable_id`) and
//! versioning (`dirty_index`) are deliberately excluded -- they are minted per
//! node (Light.cpp:44-47) and would make two identically-configured Lights
//! compare unequal for a reason that has nothing to do with shading, which
//! would destroy the comparator's value as a difference oracle.
//! Field list per 3rdparty/libossia/src/ossia/dataflow/geometry_port.hpp:
//! 840-872.
struct ShadingFields
{
  ossia::light_type type{};
  ossia::light_decay decay{};
  float color[3]{};
  float intensity{};
  float range{};
  float inner_cone_angle{};
  float outer_cone_angle{};
  float width{};
  float height{};
  float radius{};
  bool shadow_enabled{};
  float shadow_bias{};
  float shadow_normal_bias{};

  friend bool operator==(const ShadingFields& a, const ShadingFields& b) noexcept
  {
    return a.type == b.type && a.decay == b.decay && a.color[0] == b.color[0]
           && a.color[1] == b.color[1] && a.color[2] == b.color[2]
           && a.intensity == b.intensity && a.range == b.range
           && a.inner_cone_angle == b.inner_cone_angle
           && a.outer_cone_angle == b.outer_cone_angle && a.width == b.width
           && a.height == b.height && a.radius == b.radius
           && a.shadow_enabled == b.shadow_enabled
           && a.shadow_bias == b.shadow_bias
           && a.shadow_normal_bias == b.shadow_normal_bias;
  }
};

ShadingFields shading_of(const ossia::light_component& lc) noexcept
{
  ShadingFields f;
  f.type = lc.type;
  f.decay = lc.decay;
  f.color[0] = lc.color[0];
  f.color[1] = lc.color[1];
  f.color[2] = lc.color[2];
  f.intensity = lc.intensity;
  f.range = lc.range;
  f.inner_cone_angle = lc.inner_cone_angle;
  f.outer_cone_angle = lc.outer_cone_angle;
  f.width = lc.width;
  f.height = lc.height;
  f.radius = lc.radius;
  f.shadow_enabled = lc.shadow.enabled;
  f.shadow_bias = lc.shadow.bias;
  f.shadow_normal_bias = lc.shadow.normal_bias;
  return f;
}

// --- the shipped-encoder parse ----------------------------------------------

std::string trimmed(const std::string& s)
{
  const auto ws
      = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
  std::size_t b = 0, e = s.size();
  while(b < e && ws(s[b]))
    ++b;
  while(e > b && ws(s[e - 1]))
    --e;
  return s.substr(b, e - b);
}

//! Read the shipped Threedim/Light.cpp. Returns false when the source tree is
//! not at hand; callers SKIP on that -- environmental absence is never a
//! failure, and a SKIP inside an okToFail case is counted as skipped, not as
//! an unexpected pass (catch_totals.cpp:52-62).
bool read_light_cpp(std::string& out)
{
#if !defined(THREEDIM_SRC_DIR)
  return false;
#else
  const std::string path = std::string(THREEDIM_SRC_DIR) + "/Light.cpp";
  std::ifstream f(path, std::ios::binary);
  if(!f.good())
    return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
#endif
}

//! Parse the `toRawLightType` switch (Light.cpp:141-154) back into the pure
//! function it is: Light::Mode label -> the literal the encoder returns for it.
//! Fallthrough labels (Rect / Disk / Sphere today) all take the literal of the
//! `return` that terminates their group; the trailing `return 1.f;` AFTER the
//! switch belongs to no label and is not attributed to one.
//!
//! The slice deliberately covers only the encoder's own body: from the first
//! occurrence of `toRawLightType` (its definition at Light.cpp:141 -- the
//! comment above it at :137-140 does not spell the name, and the only other
//! use is at :214, further down) to the first occurrence of `toRawLightDecay`
//! (its definition at :156).
std::map<std::string, std::string> parse_encoder(const std::string& src)
{
  std::map<std::string, std::string> out;

  const std::size_t fn = src.find("toRawLightType");
  const std::size_t end = src.find("toRawLightDecay");
  if(fn == std::string::npos || end == std::string::npos || end <= fn)
    return out;

  const std::string body = src.substr(fn, end - fn);

  static constexpr const char* kCase = "case Light::";
  static constexpr std::size_t kCaseLen = 12; // strlen("case Light::")

  std::vector<std::string> pending;
  std::size_t p = 0;
  while((p = body.find(kCase, p)) != std::string::npos)
  {
    p += kCaseLen;
    const std::size_t colon = body.find(':', p);
    if(colon == std::string::npos)
      break;
    pending.push_back(trimmed(body.substr(p, colon - p)));

    const std::size_t nextCase = body.find(kCase, colon);
    const std::size_t ret = body.find("return", colon);
    if(ret != std::string::npos && (nextCase == std::string::npos || ret < nextCase))
    {
      const std::size_t semi = body.find(';', ret);
      if(semi == std::string::npos)
        break;
      const std::string lit
          = trimmed(body.substr(ret + 6, semi - (ret + 6)));
      for(const auto& label : pending)
        out[label] = lit;
      pending.clear();
    }
    p = colon;
  }
  return out;
}

//! The difference oracle used identically by the P2-11 controls and by the
//! P2-11 pin: do these two Light modes get DIFFERENT shader-facing type codes?
//! An unparsed label reports "does not separate" rather than silently passing.
bool encoder_separates(
    const std::map<std::string, std::string>& code, const char* a, const char* b)
{
  const auto ia = code.find(a);
  const auto ib = code.find(b);
  if(ia == code.end() || ib == code.end())
    return false;
  return ia->second != ib->second;
}

// =============================================================================
// P2-12 harness
// =============================================================================

//! Storage handed out as an inert reference for callee parameters the product
//! provably never dereferences: unnamed parameters at SceneFilterNode.cpp:163 /
//! :173 / :245, and `renderer` used only as `&renderer` at :194-211. Same seam
//! as tests/threedim/CameraRelease.cpp. Nothing here is ever read or written
//! by anyone -- it exists solely to have a well-aligned address.
template <typename T>
T& inert()
{
  alignas(std::max_align_t) static unsigned char storage[256]{};
  return *reinterpret_cast<T*>(&storage[0]);
}

//! A concrete NodeRenderer that does nothing but be a legal delivery target.
//! NodeRenderer has exactly four pure virtuals -- init / update / release
//! (NodeRenderer.hpp:45, :46, :55) and removeOutputPass (:87) -- all no-ops
//! here. A scene delivered to it lands in the base class's public
//! m_portScenes (NodeRenderer.cpp:590-596; member at NodeRenderer.hpp:207).
struct SceneSink final : score::gfx::NodeRenderer
{
  using score::gfx::NodeRenderer::NodeRenderer;

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }
  void release(score::gfx::RenderList&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
};

//! What survived a filter pass, in a form that compares cleanly and prints
//! usefully. Root order is preserved by SceneFilterNode.cpp:128-133, so a
//! plain ordered comparison is the right equality.
struct RootDesc
{
  uint64_t id{};
  std::string name;
  bool visible{};

  // C++20 rewrites `a != b` from this; no explicit operator!= (an explicit one
  // would be a non-rewritten candidate competing with the rewrite).
  friend bool operator==(const RootDesc& a, const RootDesc& b) noexcept
  {
    return a.id == b.id && a.name == b.name && a.visible == b.visible;
  }
};

std::vector<RootDesc> roots_of(const ossia::scene_spec& sp)
{
  std::vector<RootDesc> out;
  if(!sp.state || !sp.state->roots)
    return out;
  for(const auto& r : *sp.state->roots)
    if(r)
      out.push_back(RootDesc{r->id.value, r->name, r->visible});
  return out;
}

std::string describe(const std::vector<RootDesc>& v)
{
  std::ostringstream ss;
  ss << "{";
  for(std::size_t i = 0; i < v.size(); ++i)
    ss << (i ? ", " : "") << v[i].name << "#" << v[i].id
       << (v[i].visible ? "(visible)" : "(hidden)");
  ss << "}";
  return ss.str();
}

//! Drive the REAL score::gfx::SceneFilterNode renderer for one mode and return
//! the scene it published downstream. Path and inert-reference argument in the
//! file header; all of it is product code, none of it touches a QRhi.
ossia::scene_spec run_filter(int mode, const ossia::scene_spec& in)
{
  auto& rl = inert<score::gfx::RenderList>();
  auto& batch = inert<QRhiResourceUpdateBatch>();
  auto& cb = inert<QRhiCommandBuffer>();

  score::gfx::SceneFilterNode filterNode;
  filterNode.m_mode = mode;

  // Any concrete score::gfx::Node with a Scene input port will do as the
  // downstream; a second SceneFilterNode is the cheapest one in reach
  // (its input[0] is the Types::Scene port, SceneFilterNode.cpp:221).
  score::gfx::SceneFilterNode sinkNode;
  SceneSink sink{sinkNode};
  sinkNode.renderedNodes.emplace(&rl, &sink);

  ossia::scene_spec result;
  {
    score::gfx::Edge edge{
        filterNode.output[0], sinkNode.input[0],
        Process::CableType::ImmediateStrict};

    auto* renderer = filterNode.createRenderer(rl);
    REQUIRE(renderer != nullptr);

    renderer->scene = in;
    renderer->sceneChanged = true;
    renderer->init(rl, batch);
    renderer->update(rl, batch, nullptr);

    QRhiResourceUpdateBatch* res = nullptr;
    renderer->runInitialPasses(rl, cb, res, edge);

    renderer->release(rl);
    delete renderer;
    // `edge` dies here, before the nodes it points into: ~Edge unlinks itself
    // from both ports (Utils.hpp:113-121) and ~Node deletes them (Node.cpp:
    // 16-22).
  }

  for(const auto& [key, spec] : sink.m_portScenes)
    if(key.first == 0)
      result = spec;
  return result;
}

//! The scene both P2-12 behaviour cases run on. TWO ROOTS WITH THE SAME NAME,
//! differing only in `visible`. Rationale in the file header: any name-based
//! predicate is blind to `visible` and must keep both or neither, while mode 1
//! keeps exactly one -- so "mode 2 != mode 1" is required for every possible
//! substring, including the empty one, and the assertions need no access to
//! the "Name" control that does not exist yet.
ossia::scene_spec make_same_name_scene()
{
  auto hidden = std::make_shared<ossia::scene_node>();
  hidden->id.value = 1;
  hidden->name = "obj";
  hidden->visible = false;
  hidden->children = std::make_shared<std::vector<ossia::scene_payload>>();

  auto shown = std::make_shared<ossia::scene_node>();
  shown->id.value = 2;
  shown->name = "obj";
  shown->visible = true;
  shown->children = std::make_shared<std::vector<ossia::scene_payload>>();

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{hidden, shown});
  state->version = 1;

  ossia::scene_spec sp;
  sp.state = std::move(state);
  return sp;
}

} // namespace

// =============================================================================
// P2-11 -- UNTAGGED CONTROLS
// =============================================================================

// The spec row's premise is only half true, and this is the half that is
// ALREADY CORRECT: at the producer level a rect light and a point light are
// genuinely different objects. Written GREEN, not pinned. It is also the
// discrimination control for the ShadingFields comparator -- it shows the
// comparator says "same" for two identically-configured Lights and "different"
// for a pair separated by one slider, so a later "different" verdict means
// something.
TEST_CASE(
    "P2-11 (producer, GREEN): a rect light already differs from a point light "
    "in the scene_spec",
    "[threedim][light][p2-11][control]")
{
  // --- the oracle must be able to say "same" -------------------------------
  // Two independently-built Lights with identical controls compare equal on
  // every shading-relevant field. If this fails, ShadingFields is picking up
  // per-instance identity and every "different" verdict below is worthless.
  {
    LightProbe a{Threedim::Light::Point};
    LightProbe b{Threedim::Light::Point};
    const auto* la = a.emit();
    const auto* lb = b.emit();
    REQUIRE(la != nullptr);
    REQUIRE(lb != nullptr);
    CHECK(shading_of(*la) == shading_of(*lb));
  }

  // --- the oracle must be able to say "different" --------------------------
  // Same mode, one slider moved: the comparator must notice.
  {
    LightProbe a{Threedim::Light::Point};
    LightProbe b{Threedim::Light::Point};
    b.node.inputs.intensity.value = 9.5f; // vs 7.5f from LightProbe's ctor
    const auto* la = a.emit();
    const auto* lb = b.emit();
    REQUIRE(la != nullptr);
    REQUIRE(lb != nullptr);
    CHECK_FALSE(shading_of(*la) == shading_of(*lb));
  }

  // --- the already-correct half of P2-11 -----------------------------------
  LightProbe rect{Threedim::Light::Rect};
  LightProbe point{Threedim::Light::Point};
  const auto* lr = rect.emit();
  const auto* lp = point.emit();
  REQUIRE(lr != nullptr);
  REQUIRE(lp != nullptr);

  // Light.cpp:18-24 maps the mode onto a distinct ossia::light_type ...
  CHECK(lr->type == ossia::light_type::rect_area);
  CHECK(lp->type == ossia::light_type::point);
  // ... and Light.cpp:70-71 carries the extent onto the component.
  CHECK(lr->width == Approx(4.f));
  CHECK(lr->height == Approx(2.f));
  // Both sit at the same place -- "a point light at its centre" -- and are
  // still not the same light.
  CHECK_FALSE(shading_of(*lr) == shading_of(*lp));

  // Disk, Sphere and Dome likewise reach the scene_spec as themselves.
  LightProbe disk{Threedim::Light::Disk};
  LightProbe sphere{Threedim::Light::Sphere};
  LightProbe dome{Threedim::Light::Dome};
  LightProbe dir{Threedim::Light::Directional};
  const auto* ld = disk.emit();
  const auto* ls = sphere.emit();
  const auto* ldome = dome.emit();
  const auto* ldir = dir.emit();
  REQUIRE(ld != nullptr);
  REQUIRE(ls != nullptr);
  REQUIRE(ldome != nullptr);
  REQUIRE(ldir != nullptr);
  CHECK(ld->type == ossia::light_type::disk_area);
  CHECK(ls->type == ossia::light_type::sphere_area);
  CHECK(ldome->type == ossia::light_type::dome);
  CHECK(ldir->type == ossia::light_type::directional);
  CHECK(ld->radius == Approx(3.f)); // Light.cpp:72
  CHECK_FALSE(shading_of(*ldome) == shading_of(*ldir));
}

// The discrimination control for the PARSED-ENCODER oracle the P2-11 pin uses.
// It runs the SAME comparator on pairs whose answer is not in dispute, so a
// red pin means "the encoder collapses these" and not "the parse broke". The
// parse-integrity REQUIREs live here, untagged, on purpose: a stale parse must
// redden something visible rather than quietly satisfying an expected-to-fail
// entry.
TEST_CASE(
    "P2-11 control: the shipped encoder parse can say BOTH same and different",
    "[threedim][light][p2-11][control]")
{
  // The 64-byte budget the file header's fix note rests on: every byte of
  // RawLightData is already spoken for, so a complete fix to P2-11 must widen
  // the record and not only the encoder. Pinned by a static_assert at
  // SceneGPUState.hpp:345; restated here because that is the constraint a
  // fixer will hit first.
  CHECK(sizeof(score::gfx::RawLightData) == 64);

  std::string src;
  if(!read_light_cpp(src))
    SKIP("THREEDIM_SRC_DIR/Light.cpp not readable: no source tree to parse");

  const auto code = parse_encoder(src);

  // Parse integrity: all seven Light::Mode enumerators (Light.hpp:55-64) must
  // have been recovered, or the oracle is reading nothing at all.
  INFO("parsed toRawLightType (Light.cpp:141-154), " << code.size() << " labels");
  REQUIRE(code.size() == 7);
  for(const char* m :
      {"Directional", "Point", "Spot", "Rect", "Disk", "Sphere", "Dome"})
  {
    INFO("missing case label: " << m);
    REQUIRE(code.count(m) == 1);
  }

  // The oracle must be able to say DIFFERENT. These pairs are separated by the
  // shipped encoder today (Light.cpp:145-147) and must stay separated: point,
  // directional and spot are the three codes the shader branches on.
  CHECK(encoder_separates(code, "Point", "Directional"));
  CHECK(encoder_separates(code, "Spot", "Point"));
  CHECK(encoder_separates(code, "Spot", "Directional"));

  // The oracle must be able to say SAME. Rect and Disk share `return 1.f;` via
  // fallthrough (Light.cpp:148-150), so a comparator that could only ever
  // report "different" is caught right here.
  //
  // NOTE this one CHECK is a property of the PARSER, not a contract on the
  // product: a correct fix giving Rect and Disk distinct codes will redden it.
  // That is expected -- when the pin below goes green, either repoint this at
  // some other pair the parser recovers as equal, or delete it and let the
  // three "different" CHECKs above carry the discrimination alone.
  CHECK_FALSE(encoder_separates(code, "Rect", "Disk"));
}

// =============================================================================
// P2-11 -- THE PIN
// =============================================================================

// EXPECTED RED. Asserts the CORRECT behaviour, never today's.
//
// The shader-facing per-light type code (RawLightData::local_direction.w,
// SceneGPUState.hpp:311-320) must be INJECTIVE over Light::Mode: a Rect, Disk
// or Sphere light must not arrive at the shader wearing a point light's code,
// and a Dome light must not arrive wearing a directional light's code. Today
// the encoder deliberately collapses them -- Light.cpp:137-140 says so in
// words, Light.cpp:148-151 does it in code -- so all four CHECKs fail.
//
// Flips to a plain pass the day toRawLightType stops collapsing. See the file
// header for why a complete fix must also widen RawLightData, and for why this
// pin reads the shipped source instead of calling the encoder.
TEST_CASE(
    "DEFECT P2-11: the render-thread light encoder collapses area lights onto "
    "point, and dome onto directional",
    "[threedim][light][p2-11][known-defect][!shouldfail]")
{
  std::string src;
  if(!read_light_cpp(src))
    SKIP("THREEDIM_SRC_DIR/Light.cpp not readable: no source tree to parse");

  const auto code = parse_encoder(src);
  REQUIRE(code.size() == 7); // integrity; the untagged control owns the detail

  INFO("Light.cpp:137-140 admits it: \"Area / dome modes collapse onto "
       "punctual analogues for the raw arena (directional for dome, point for "
       "rect / disk / sphere) -- area-light shading is a shader-side "
       "extension.\"");
  INFO("Light.hpp:41-42 admits it too: area-light shapes \"are rendered as "
       "point-light approximations until shaders add the Rect/Disk/Sphere "
       "sampling math.\"");

  // A rect light of a given extent must not compute to a point light at its
  // centre. Same for disk and sphere.
  CHECK(encoder_separates(code, "Rect", "Point"));
  CHECK(encoder_separates(code, "Disk", "Point"));
  CHECK(encoder_separates(code, "Sphere", "Point"));
  // ... and a dome light must not compute to a directional light.
  CHECK(encoder_separates(code, "Dome", "Directional"));
}

// =============================================================================
// P2-12 -- UNTAGGED CONTROL, then the GREEN case the spec row asked for
// =============================================================================

// The discrimination control for the P2-12 oracle, run through the EXACT
// harness the behaviour cases use: same scene, same run_filter(), same
// comparator. Two genuinely different modes must come out different, and one
// mode run twice must come out identical. Without this, any "these two modes
// agree" verdict could just mean the harness delivers nothing.
TEST_CASE(
    "P2-12 control: the harness discriminates between two genuinely different "
    "modes",
    "[gfx][scene][scenefilter][p2-12][control]")
{
  const auto scene = make_same_name_scene();

  const auto m0 = roots_of(run_filter(0, scene));
  const auto m1 = roots_of(run_filter(1, scene));

  INFO("mode 0 -> " << describe(m0));
  INFO("mode 1 -> " << describe(m1));

  // The harness delivers something at all.
  REQUIRE(m0.size() == 2);

  // Mode 0 is pass-through (SceneFilterNode.cpp:121-123): both roots survive.
  CHECK(m0[0] == RootDesc{1, "obj", false});
  CHECK(m0[1] == RootDesc{2, "obj", true});

  // Mode 1 drops the invisible subtree (SceneFilterNode.cpp:45-47): exactly
  // the visible root survives.
  REQUIRE(m1.size() == 1);
  CHECK(m1[0] == RootDesc{2, "obj", true});

  // ORACLE, "different" direction: two different modes, different answers.
  CHECK(m0 != m1);

  // ORACLE, "same" direction: the same mode twice gives the same answer, so an
  // inequality elsewhere is a statement about the modes and not about
  // run-to-run noise in the harness.
  const auto m1_again = roots_of(run_filter(1, scene));
  CHECK(m1 == m1_again);
}

// **NOT A PIN.** Spec row P2-12 asks for `[!shouldfail]` here on the strength
// of SceneFilterNode.hpp:22 ("behaves like mode 1 until wired"). That clause is
// stale: the mode is consulted only at SceneFilterNode.cpp:46 (`mode == 1`, a
// literal) and :122 (`mode == 0`), so mode 2 takes no filtering branch, keeps
// invisible subtrees, and already differs from mode 1 on any scene that has
// one. The contract the row wants is satisfied -- so it is asserted GREEN.
//
// Worth keeping despite being green: the most natural "fix" to that stale
// doc-comment is to widen :46 to `mode != 0`, which would make mode 2 into
// mode 1 for real. This case is what would catch that.
TEST_CASE(
    "P2-12 (GREEN): SceneFilterNode mode 2 already is not mode 1",
    "[gfx][scene][scenefilter][p2-12]")
{
  const auto scene = make_same_name_scene();
  const auto m0 = roots_of(run_filter(0, scene));
  const auto m1 = roots_of(run_filter(1, scene));
  const auto m2 = roots_of(run_filter(2, scene));

  INFO("mode 0 -> " << describe(m0));
  INFO("mode 1 -> " << describe(m1));
  INFO("mode 2 -> " << describe(m2));
  INFO("SceneFilterNode.hpp:22 claims mode 2 'behaves like mode 1 until "
       "wired'; SceneFilterNode.cpp:46 says otherwise");

  REQUIRE(m1.size() == 1);

  // The spec row's contract, and the contract every correct implementation
  // must keep: a name predicate cannot see `visible`, so on two roots both
  // named "obj" mode 2 must keep 2 or keep 0 -- never mode 1's 1.
  CHECK(m2 != m1);
  CHECK(m2.size() != 1u);

  // Where mode 2 actually lands today: on mode 0's kept set, because no
  // predicate fires. Not a contract on the final behaviour -- a correct name
  // filter with an empty default substring keeps this true, a non-empty
  // default would not -- but it pins the current answer so drift is reported
  // rather than silent. Mode 2 is still not mode 0 as a whole: rewrite() copies
  // the state and bumps version/dirty_index for any non-zero mode
  // (SceneFilterNode.cpp:126-141), which the kept-set comparison ignores.
  CHECK(m2 == m0);
}

// =============================================================================
// P2-12 -- THE RE-SCOPED PIN
// =============================================================================

// EXPECTED RED, on the half of SceneFilterNode.hpp:20-22 that is still true:
// "string port missing in the renderer for now".
//
// SceneFilterNode's constructor creates exactly two input ports, Scene and
// Mode (SceneFilterNode.cpp:219-227). Ports are a score::gfx::Node's only
// input channel (Node.hpp:103, fed by Node::process(Message&&)), so mode 2 --
// documented as "keep only subtrees whose node name contains the substring set
// in the 'Name' control" -- cannot be given a substring by any route. It is
// not merely unwired, it is unconfigurable, and no test anywhere can drive its
// real behaviour until this changes. That is why the row's own contract had to
// be asserted in the substring-agnostic form above.
//
// The correct behaviour: SceneFilterNode must expose a Name input alongside
// Scene and Mode. The pin deliberately does not constrain that port's TYPE --
// score::gfx::Types has no string kind today (Uniforms.hpp:10-24: Empty, Int,
// Float, Vec2, Vec3, Vec4, Image, Audio, Camera, Geometry, Buffer, Scene), so
// which type it ends up with is UNVERIFIED and up to whoever adds it. Flips to
// a plain pass the day a third input port lands.
TEST_CASE(
    "DEFECT P2-12 (re-scoped): SceneFilterNode mode 2 has no Name port, so it "
    "cannot be configured at all",
    "[gfx][scene][scenefilter][p2-12][known-defect][!shouldfail]")
{
  score::gfx::SceneFilterNode node;

  // Today: [0] = Scene, [1] = Mode (SceneFilterNode.cpp:221-225).
  REQUIRE(node.input.size() >= 2);
  REQUIRE(node.input[0] != nullptr);
  REQUIRE(node.input[1] != nullptr);
  CHECK(node.input[0]->type == score::gfx::Types::Scene);
  CHECK(node.input[1]->type == score::gfx::Types::Int);

  INFO("SceneFilterNode.hpp:20-22 admits it: mode 2 is 'keep only subtrees "
       "whose node name contains the substring set in the \"Name\" control "
       "(future-wired; string port missing in the renderer for now, so behaves "
       "like mode 1 until wired)'");
  INFO("input port count is " << node.input.size() << "; a Name input is needed "
                                                      "for mode 2 to mean anything");

  CHECK(node.input.size() >= 3u);
}
