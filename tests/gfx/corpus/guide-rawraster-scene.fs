/*{
  "DESCRIPTION": "Guide example 3 -- raw raster on the SCENE path. Placement comes from per_draws.data[draw_id].model, which is where a scene Transform3D is baked; MODEL_MATRIX is identity here by construction, so reading it instead would pin every object at the origin. draw_id arrives through the instance_draw_id semantic. The camera comes from the built-in, which indexes the camera block through VIEW_INDEX and is therefore correct under MULTIVIEW too.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["GUIDE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] }
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
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
