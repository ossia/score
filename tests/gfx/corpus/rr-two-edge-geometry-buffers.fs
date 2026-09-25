/*{
  "DESCRIPTION": "Raw raster whose uniform and storage INPUTS are name-matched against the ScenePreprocessor geometry's auxiliary buffers (camera, scene_counts). Paints the camera position and the draw count so both bindings are live.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[15]" } ]
    },
    { "NAME": "scene_counts", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "light_count",    "TYPE": "uint" },
        { "NAME": "material_count", "TYPE": "uint" },
        { "NAME": "draw_count",     "TYPE": "uint" },
        { "NAME": "_pad0",          "TYPE": "uint" }
      ]
    }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    vec3 eye = camera.data[12].xyz;
    isf_FragColor = vec4(
        clamp(eye * 0.25 + 0.5, 0.0, 1.0).xy,
        float(scene_counts.draw_count) / 4.0, 1.0);
}
