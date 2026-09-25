/*{
  "DESCRIPTION": "Fullscreen raw raster shaped like volumetric_composite: a geometry INPUT with no attributes, a DEPTH:true image input, a uniform block, material controls and a top-level AUXILIARY buffer. R = level, G = 1 when mode is 1, B = the input's green channel, A = 1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "scene", "TYPE": "geometry", "ATTRIBUTES": [] },
    { "NAME": "inputImage", "TYPE": "image", "DEPTH": true, "VISIBILITY": "fragment" },
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "fragment", "LAYOUT": [ { "NAME": "view", "TYPE": "mat4" }, { "NAME": "params", "TYPE": "vec4" } ] },
    { "NAME": "level", "TYPE": "float", "DEFAULT": 0.25, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "mode", "TYPE": "long", "DEFAULT": 0, "VALUES": [0, 1], "LABELS": ["a", "b"] }
  ],
  "AUXILIARY": [ { "NAME": "cluster_config", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "cluster_x", "TYPE": "uint" } ] } ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/

void main()
{
    vec3 src = texture(inputImage, v_uv).rgb;
    float d = texture(inputImage_depth, v_uv).r;
    isf_FragColor = vec4(level, mode == 1 ? 1.0 : 0.0, src.g + 0.0 * d + 0.0 * camera.params.x + 0.0 * float(cluster_config.cluster_x), 1.0);
}
