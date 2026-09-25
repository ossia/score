/*{
  "DESCRIPTION": "RAW_RASTER_PIPELINE scene consumer placing each instance as the library rasterizers do with per-instance matrices: per_draws.data[draw_id].model * (instLinear * position + inst_translation), drawn in the instance custom colour or white. Used by GfxInstancerPlacementI2.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-INSTANCING"],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "CULL_MODE": "none" },
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "vec3", "NAME": "inst_translation", "SEMANTIC": "translation", "REQUIRED": false },
    { "TYPE": "vec4", "NAME": "inst_custom0", "SEMANTIC": "instance_custom0", "REQUIRED": false },
    { "TYPE": "vec4", "NAME": "inst_matrix0", "REQUIRED": false },
    { "TYPE": "vec4", "NAME": "inst_matrix1", "REQUIRED": false },
    { "TYPE": "vec4", "NAME": "inst_matrix2", "REQUIRED": false },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_custom" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_custom" } ],
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
    isf_FragColor = vec4(v_custom.rgb, 1.0);
}
