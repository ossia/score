/*{
  "CREDIT": "ossia team",
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "DESCRIPTION": "Smallest possible RAW_RASTER_PIPELINE -> cubemap test. Writes six distinct solid colours, one per face, via MULTIVIEW:6 + CUBEMAP:true. Use to isolate whether the runtime's cubemap+multiview cube-copy shim is working independently of any convolution maths. Expected result: sampling the cube you see 6 flat-colour faces (+X red, -X cyan, +Y green, -Y magenta, +Z blue, -Z yellow). If you see fewer faces or black, the shim has a problem. Lifted verbatim from 2026/test-cubemap-output.score, with an explicit 64x64 OUTPUT size added so the cube face edge does not depend on the render-target size (the runtime forces cube faces square via min(w,h) otherwise). Pairs with syn-cube-six-colors.vs and the syn-cube-six-probe.fs samplerCube viewer.",
  "CATEGORIES": ["3D", "Debug", "Test", "TEST-CUBEMAP", "TEST-MULTIVIEW"],

  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [
    { "TYPE": "int", "NAME": "v_face" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "int", "NAME": "v_face" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],

  "OUTPUTS": [
    { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 6, "CUBEMAP": true, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "MULTIVIEW": 6,

  "INPUTS": [],

  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/

// Colour table — one per face, matching the QRhi / GL cubemap layer
// ordering (0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z). Complementary colour
// pairs across opposite faces so it's easy to spot a flipped axis.
const vec3 kFaceColor[6] = vec3[6](
    vec3(1.0, 0.0, 0.0),   // +X red
    vec3(0.0, 1.0, 1.0),   // -X cyan
    vec3(0.0, 1.0, 0.0),   // +Y green
    vec3(1.0, 0.0, 1.0),   // -Y magenta
    vec3(0.0, 0.0, 1.0),   // +Z blue
    vec3(1.0, 1.0, 0.0)    // -Z yellow
);

void main()
{
    int f = clamp(v_face, 0, 5);
    isf_FragColor = vec4(kFaceColor[f], 1.0);
}
