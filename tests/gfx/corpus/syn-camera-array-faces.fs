/*{
  "DESCRIPTION": "P2-1 probe: MULTIVIEW:6 + CUBEMAP:true raw-raster pass whose ONLY input is the ScenePreprocessor's `camera` auxiliary UBO, read as an ARRAY and indexed by gl_ViewIndex. Face f is painted with the encoded world-space forward direction of camera[f]: rgb = forward*0.5+0.5. A Threedim::CameraArray upstream therefore paints +X (255,128,128), -X (0,128,128), +Y (128,255,128), -Y (128,0,128), +Z (128,128,255), -Z (128,128,0). Unlike syn-cube-six-colors.fs -- which colours by gl_ViewIndex alone and so cannot tell a camera array from a constant table -- every channel here comes out of the camera UBO, so the pass fails if the six views do not each get their own camera. Pairs with syn-camera-array-faces.vs and is read back through the existing syn-cube-six-probe.fs samplerCube viewer.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["3D", "Debug", "Test", "TEST-SCENE", "TEST-CUBEMAP", "TEST-MULTIVIEW"],

  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec3", "NAME": "v_color" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec3", "NAME": "v_color" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],

  "OUTPUTS": [
    { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 6, "CUBEMAP": true, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "MULTIVIEW": 6,

  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "data", "TYPE": "vec4[90]" }
      ]
    }
  ],

  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "TOPOLOGY": "triangles"
  }
}*/

// ---------------------------------------------------------------------------
// Why `vec4[90]` and not a struct array
// ---------------------------------------------------------------------------
// libisf's uniform_input emitter writes one GLSL declaration per LAYOUT entry
// and supports an array suffix in the TYPE string (isf.cpp:3359-3366 splits on
// '['), but it has no way to declare a nested struct type. The block is
// therefore declared as the raw std140 image of CameraUBOData[6].
//
// CameraUBOData (Gfx/Graph/CameraMath.hpp:23-32, sizeof pinned to 240 by the
// static_assert at :32 and re-pinned by tests/threedim/KnownDefects.cpp:161-168):
//     float view[16];            //   0 ..  63   -> vec4 0..3   (4 columns)
//     float projection[16];      //  64 .. 127   -> vec4 4..7
//     float viewProjection[16];  // 128 .. 191   -> vec4 8..11
//     float cameraPosition[4];   // 192 .. 207   -> vec4 12
//     float renderSize[4];       // 208 .. 223   -> vec4 13
//     float params[4];           // 224 .. 239   -> vec4 14
// = 15 vec4. In std140 a vec4 array has stride 16, so vec4[90] is byte-for-byte
// CameraUBOData[6] with no padding anywhere, and entry i starts at vec4 15*i.
//
// The declared block is 6 * 240 = 1440 bytes. That is exactly the extent
// ScenePreprocessorNode advertises for the "camera" auxiliary now that issue
// #163 is fixed: both publication sites pass cameraAuxByteSize(N cameras)
// (ScenePreprocessorNode.cpp:1673 and :2802 -> CameraMath.hpp:36-41), and the
// consumer takes the aux's byte_size over the wrapped buffer's own size
// (RenderedRawRasterPipelineNode.cpp:1739). Before that fix the binding was
// 240 bytes wide against this 1440-byte block.
//
// The camera block is vertex-only (VISIBILITY: "vertex"): the fragment stage
// needs nothing from it, and keeping it out of the fragment stage keeps this
// shader inside the vertex-stage-only multiview guarantee the .vs documents.
// syn-scene-xform.fs sets the same VISIBILITY precedent.

void main()
{
    isf_FragColor = vec4(v_color, 1.0);
}
