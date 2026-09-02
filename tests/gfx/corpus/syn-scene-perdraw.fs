/*{
  "DESCRIPTION": "Encodes the SCENE path's per-object model matrix -- per_draws.data[draw_id].model, the vehicle the flattener actually fills (ScenePreprocessorNode.cpp:41-49 PerDrawGPU::model, published as the `per_draws` auxiliary at :2786) -- into the fragment colour, AND uses it to place the geometry. Companion to syn-scene-modelmat.fs, which reads MODEL_MATRIX instead: MODEL_MATRIX is the raw-raster convention, written ONLY by RenderedRawRasterPipelineNode::process(port, transform3d) (:3378) from a transform3d message delivered to the raster node's OWN port, so it is identity for anything arriving through a scene chain. Same shape as the real shaders (shadow_cascades.vert, and 5 corpus documents that index per_draws).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id",
      "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_mm" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_mm" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ]
    }
  ],
  "TYPES": [
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model",           "TYPE": "mat4" },
        { "NAME": "normal",          "TYPE": "mat4" },
        { "NAME": "material_index",  "TYPE": "uint" },
        { "NAME": "tag_hash",        "TYPE": "uint" },
        { "NAME": "transform_slot",  "TYPE": "uint" },
        { "NAME": "skeleton_offset", "TYPE": "uint" }
    ] }
  ]
}*/

void main()
{
    // +0.5 bias so a translation of 0 reads mid-grey and a negative one is visible.
    isf_FragColor = vec4(v_mm.x + 0.5, v_mm.y + 0.5, v_mm.z + 0.5, 1.0);
}
