/*{
  "DESCRIPTION": "Procedural MULTIVIEW:2 raw-raster with a graphics uniform_input `cam`: the SRB must give `cam` its declared binding next to the multiview plumbing (it once collided with an engine multiview UBO, since removed). `cam` need not be referenced in GLSL: its INPUTS declaration alone makes the codegen emit the binding. A fullscreen triangle writes a per-view colour so layer 0 (VIEW_INDEX 0) reads back reddish.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-MULTIVIEW", "TEST-BINDING"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "OUTPUTS": [
    { "NAME": "views", "TYPE": "color", "FORMAT": "rgba8", "LAYERS": 2, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "MULTIVIEW": 2,
  "INPUTS": [
    { "NAME": "cam", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "tint", "TYPE": "vec4" }
      ]
    }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/

void main()
{
    // Per-view colour. Layer 0 (VIEW_INDEX 0) -> reddish; layer 1 -> greenish.
    vec3 base = (VIEW_INDEX == 0) ? vec3(0.9, 0.2, 0.2) : vec3(0.2, 0.9, 0.2);
    isf_FragColor = vec4(base, 1.0);
}
