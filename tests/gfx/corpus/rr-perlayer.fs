/*{
  "DESCRIPTION": "Procedural RAW_RASTER_PIPELINE with EXECUTION_MODEL=PER_LAYER over a 4-layer color TextureArray output. No geometry input (fullscreen triangle from gl_VertexIndex). Each of the 4 invocations binds one array layer and writes a distinct color keyed on PASSINDEX (the layer index). Exercises the per-invocation UBO/SRB pool (R3-N7): all 4 layers must render without a UAF/validation error, and layer 0 (PASSINDEX 0) reads back as its expected color.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-EXECUTION-MODEL"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "OUTPUTS": [
    { "NAME": "layered", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 4, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "layered" },
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  },
  "INPUTS": []
}*/

void main()
{
    float l = float(PASSINDEX);           // layer index, 0..3
    isf_FragColor = vec4(l / 3.0, 1.0 - l / 3.0, 0.25, 1.0);
}
