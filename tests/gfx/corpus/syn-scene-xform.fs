/*{
  "DESCRIPTION": "Draws scene geometry the way the scene path actually delivers it: per-object matrices come from the instance_transforms aux indexed by gl_InstanceIndex, and the view/projection from the camera uniform the flattener fills. Shades flat white so a pixel oracle measures placement only. syn-scene-solid.fs reads MODEL_MATRIX instead, which is the raw-raster convention and is NOT fed by a scene chain -- geometry drawn through it lands at identity no matter what the scene graph says.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "view",           "TYPE": "mat4" },
        { "NAME": "projection",     "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" },
        { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize",     "TYPE": "vec4" },
        { "NAME": "params",         "TYPE": "vec4" }
      ]
    },
    { "NAME": "instance_transforms", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "mat4[]" } ]
    }
  ]
}*/

void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
