/*{
  "DESCRIPTION": "Procedural MULTIVIEW:2 raw-raster with a graphics uniform_input `cam`. The whole point is the SRB binding layout: with a MULTIVIEW>=2 shader AND a graphics uniform_input UBO, libisf's codegen places the multiview UBO at the binding AFTER all storage INCLUDING uniform_input UBOs, while the pre-fix runtime computed the multiview binding from ssbos/images only — ignoring `cam` — so the multiview UBO collided with `cam`'s slot and the shader's real multiview binding was left without an SRB descriptor (Vulkan/D3D12 missing-descriptor crash; GL aliasing collapses geometry). `cam` need not be referenced in GLSL — its INPUTS declaration alone makes the codegen emit the binding the runtime must account for. A fullscreen triangle writes a per-view colour so layer 0 (VIEW_INDEX 0) reads back reddish when the SRB is correct.",
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
