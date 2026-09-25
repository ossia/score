/*{
  "DESCRIPTION": "Camera-driven EXECUTION_MODEL PER_CUBE_FACE cube capture: face f is rendered in pass f through camera[f] of the ScenePreprocessor's camera array and every texel stores the world direction it looks along, rgb = dir * 0.5 + 0.5. Read back through cb-cube-dir-probe.fs, a correct capture returns each probe direction's own encoding.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-CUBEMAP", "TEST-EXECUTION-MODEL"],

  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_near" },
    { "TYPE": "vec4", "NAME": "v_far" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_near" },
    { "TYPE": "vec4", "NAME": "v_far" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],

  "OUTPUTS": [
    { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 6, "CUBEMAP": true, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "cube" },

  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "data", "TYPE": "vec4[90]" }
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
    vec3 d = normalize(v_far.xyz / v_far.w - v_near.xyz / v_near.w);
    isf_FragColor = vec4(d * 0.5 + 0.5, 1.0);
}
