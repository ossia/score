/*{
  "DESCRIPTION": "NDC-derived EXECUTION_MODEL PER_CUBE_FACE cube capture: every texel of face f stores the direction the GL cube-map face table (4.6, Table 8.19) assigns to it, computed from the pre-clipSpaceCorrMatrix NDC position (v_uv) with row 0 of the face at v_uv.y = 0, rgb = dir * 0.5 + 0.5. The cubeFaceDir of the csf-examples IBL presets. Back faces are culled; the fullscreen triangle of cb-cube-dir-ndc-perface.vs is counter-clockwise in that NDC, so it is front-facing only when the pipeline front face follows the clip-space y convention.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-CUBEMAP", "TEST-EXECUTION-MODEL"],

  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" },
    { "TYPE": "int",  "NAME": "v_face" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" },
    { "TYPE": "int",  "NAME": "v_face" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],

  "OUTPUTS": [
    { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 6, "CUBEMAP": true, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "cube" },

  "INPUTS": [],

  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "back",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/

vec3 cubeFaceDir(int face, vec2 uv)
{
    vec2 xy = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    vec3 d;
    if      (face == 0) d = vec3( 1.0,  xy.y, -xy.x);
    else if (face == 1) d = vec3(-1.0,  xy.y,  xy.x);
    else if (face == 2) d = vec3( xy.x,  1.0, -xy.y);
    else if (face == 3) d = vec3( xy.x, -1.0,  xy.y);
    else if (face == 4) d = vec3( xy.x,  xy.y,  1.0);
    else                d = vec3(-xy.x,  xy.y, -1.0);
    return normalize(d);
}

void main()
{
    isf_FragColor = vec4(cubeFaceDir(v_face, v_uv) * 0.5 + 0.5, 1.0);
}
