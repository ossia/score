/*{
  "CREDIT": "ossia team",
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "DESCRIPTION": "Depth-only shadow-map pass for cascaded shadow maps. Renders the scene's indexed MDI geometry from one cascade's light-space view_projection. EXECUTION_MODEL=PER_LAYER drives the runtime to issue 8 draws (one per layer of the depth array OUTPUT), passing the iteration index through ProcessUBO.passIndex which the vertex shader picks up as the cascade index. Output is a depth-only Texture2DArray (D32F, 8 layers, 2048²). Wire through SceneResourceRoute(target=ShadowMapArray) into classic_pbr_shadowed.frag's shadow_map_array aux.",
  "CATEGORIES": ["3D", "Scene", "MDI", "Depth", "Shadow"],

  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id",
      "SEMANTIC": "instance_draw_id" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [],

  "OUTPUTS": [
    { "NAME":   "shadow",
      "TYPE":   "depth",
      "FORMAT": "d32f",
      "LAYERS": 8,
      "WIDTH":  2048,
      "HEIGHT": 2048 }
  ],

  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "shadow" },

  "INPUTS": [
    { "NAME": "shadow_cascades", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "light_view_proj",         "TYPE": "mat4[8]" },
        { "NAME": "cascade_split_distances", "TYPE": "vec4" },
        { "NAME": "cascade_count",           "TYPE": "uint" },
        { "NAME": "_pad0",                   "TYPE": "uint" },
        { "NAME": "_pad1",                   "TYPE": "uint" },
        { "NAME": "_pad2",                   "TYPE": "uint" }
      ]
    },

    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ]
    },

    { "NAME": "indirect_draw_cmds", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "none",
      "BUFFER_USAGE": "indirect_draw_indexed",
      "LAYOUT": [ { "NAME": "cmds", "TYPE": "DrawIndexedCmd[]" } ]
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
    ] },
    { "NAME": "DrawIndexedCmd", "LAYOUT": [
        { "NAME": "indexCount",   "TYPE": "uint" },
        { "NAME": "instanceCount","TYPE": "uint" },
        { "NAME": "firstIndex",   "TYPE": "uint" },
        { "NAME": "baseVertex",   "TYPE": "int"  },
        { "NAME": "baseInstance", "TYPE": "uint" }
    ] }
  ],

  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "COLOR_WRITE": false,
    "CULL_MODE": "front"
  }
}*/

// Fragment stage: empty — depth test + depth write alone are the goal.
// (The GLSL requires a main() but it never writes isf_FragColor since
// COLOR_WRITE is disabled.)
void main() { }
