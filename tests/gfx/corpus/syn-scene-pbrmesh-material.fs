/*{
  "DESCRIPTION": "Draws the flattened scene's geometry in the base colour of its material, read from the scene_materials buffer the Scene Preprocessor publishes (MaterialGPU is 80 bytes: five vec4, baseColor first) at the per_draws material index. Used by GfxPBRMeshUpstreamGeometry to see whether a scene_materials auxiliary arriving on PBR Mesh's input geometry shadows the downstream preprocessor's own.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id",
      "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" },
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ]
    },
    { "NAME": "scene_materials", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ]
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
    isf_FragColor = vec4(v_color.rgb, 1.0);
}
