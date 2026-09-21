/*{
  "DESCRIPTION": "syn-modelmat-tri.fs plus MULTIVIEW:6 and a cubemap output -- the ONE variable under study. isf.cpp reserves a binding slot for the multiview UBO when MULTIVIEW >= 2 and bumps model_ubo_binding past it, so this shader declares model_material_t one slot higher than the non-multiview control does. If the raw-raster renderer does not reserve the same slot, MODEL_MATRIX is read from the wrong binding and the six faces stop being mid-grey. Read back through syn-cube-six-probe.fs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-MULTIVIEW", "TEST-CUBEMAP"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec3", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec3", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 6, "CUBEMAP": true, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "MULTIVIEW": 6,
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(v_color, 1.0);
}
