/*{
  "DESCRIPTION": "Draws the flattened scene in solid green after linear-blend skinning in the vertex stage: JOINTS_0 / WEIGHTS_0 blend joint_matrices entries from the draw's skeleton_offset, and the skinned x/y are used as clip coordinates.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uvec4", "NAME": "joints_0" },
    { "TYPE": "vec4", "NAME": "weights_0" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" },
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] },
    { "NAME": "joint_matrices", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "mats", "TYPE": "mat4[]" } ] }
  ],
  "TYPES": [
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model", "TYPE": "mat4" },
        { "NAME": "normal", "TYPE": "mat4" },
        { "NAME": "material_index", "TYPE": "uint" },
        { "NAME": "tag_hash", "TYPE": "uint" },
        { "NAME": "transform_slot", "TYPE": "uint" },
        { "NAME": "skeleton_offset", "TYPE": "uint" }
    ] }
  ]
}*/
void main()
{
    isf_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
