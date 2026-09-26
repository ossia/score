/*{
  "DESCRIPTION": "Scene filter: zeroes instanceCount of every indirect_draw_cmds entry of its scene input, in place, the way presets/filters/scene_filter_*.csf do. Used by GfxSceneFilterIndirectE4.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "scene", "TYPE": "geometry",
      "ATTRIBUTES": [],
      "AUXILIARY": [
        { "NAME": "indirect_draw_cmds", "ACCESS": "read_write",
          "LAYOUT": [ { "NAME": "cmds", "TYPE": "DrawIndexedCmd[]" } ]
        }
      ]
    }
  ],
  "TYPES": [
    { "NAME": "DrawIndexedCmd", "LAYOUT": [
        { "NAME": "indexCount",   "TYPE": "uint" },
        { "NAME": "instanceCount","TYPE": "uint" },
        { "NAME": "firstIndex",   "TYPE": "uint" },
        { "NAME": "baseVertex",   "TYPE": "int"  },
        { "NAME": "baseInstance", "TYPE": "uint" }
    ] }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1],
      "EXECUTION_MODEL": { "TYPE": "1D_BUFFER", "TARGET": "$COUNT_indirect_draw_cmds" }
    }
  ]
}*/

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= uint(scene_indirect_draw_cmds.cmds.length()))
        return;
    scene_indirect_draw_cmds.cmds[i].instanceCount = 0u;
}
