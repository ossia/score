// P2-1 probe, vertex stage. Pairs with syn-camera-array-faces.fs.
//
// MULTIVIEW:6 fires this once per view; gl_ViewIndex selects the layer the
// fragment stage writes into AND the camera entry read out of the
// ScenePreprocessor's `camera` auxiliary UBO. The whole oracle is computed
// HERE, in the vertex stage, and handed to the fragment stage as a plain
// vec3 varying, for two reasons:
//
//  1. gl_ViewIndex is only reliable in the vertex stage on this tree: Qt's
//     SPIR-V shader tool applies ovr_multiview_view_count to the vertex stage
//     only (qspirvshader.cpp:954 -- cited by SPEC-SCENE-RENDER-TESTS.md's
//     "READ LEDGER-DEFECT-FIXES.md FIRST" preamble, which explicitly tells
//     P1-7 and P2-1 not to assume fragment-stage VIEW_INDEX bakes on GL).
//     syn-cube-six-colors.vs sets the same precedent (it computes v_face in
//     the vertex stage).
//  2. All three vertices of the fullscreen triangle carry the identical
//     value, so interpolation is a no-op and no `flat` qualifier is needed.
//
// Coverage: a fullscreen triangle synthesised from gl_VertexIndex, NOT from
// the bound mesh. The geometry edge exists only so the raw-raster node has an
// upstream geometry to name-match the "camera" auxiliary against
// (RenderedRawRasterPipelineNode.cpp:1725-1741, try_bind_from_geometry); WHERE
// the mesh lands is not what P2-1 measures, and a synthesised triangle removes
// any dependence on the cube's placement or on depth state. `position` is kept
// live (multiplied by 0.0) so the declared VERTEX_INPUTS binding is used.
void main()
{
    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);

    // camera.data is the raw std140 image of CameraUBOData[N] declared as a
    // flat vec4 array; see the .fs header for the byte-offset derivation.
    // 15 vec4 = 240 B = one CameraUBOData (CameraMath.hpp:23-32).
    int base = gl_ViewIndex * 15;

    // view = inverse(worldTransform) (CameraMath.cpp:15). For a rigid
    // transform W = [R | t], view's rotation part is R^T. In column-major
    // GLSL, view[c][r] = (R^T)[r][c] = R[c][r], so
    //   vec3(view[0][2], view[1][2], view[2][2]) == R's column 2
    //                                            == the camera's local +Z in world.
    // CameraArray builds each face with QQuaternion::fromDirection(-forward, up)
    // (CameraArray.hpp:154), i.e. local +Z maps to -forward, so
    //   forward = -column2.
    // view's four columns are data[base+0 .. base+3]; column c's z component
    // is data[base+c].z.
    vec3 fwd = -vec3(
        camera.data[base + 0].z,
        camera.data[base + 1].z,
        camera.data[base + 2].z);

    // Encoding: each component of a unit axis direction is exactly -1, 0 or +1,
    // so fwd*0.5+0.5 lands on exactly 0.0, 0.5 or 1.0 -- an 8-bit-exact,
    // sign-carrying, self-inverting encoding. See the .fs header for the six
    // colours this yields.
    v_color = fwd * 0.5 + 0.5;

    gl_Position = vec4(ndc + vec2(0.0) * position.x, 0.0, 1.0);
}
