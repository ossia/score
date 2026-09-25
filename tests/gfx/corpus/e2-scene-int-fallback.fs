/*{
  "DESCRIPTION": "Draws the flattened scene in green when the optional uvec4 joints_0 input reads its DEFAULT (3, 0, 0, 7), red otherwise.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uvec4", "NAME": "joints_0", "REQUIRED": false, "DEFAULT": [3, 0, 0, 7] }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "uvec4", "NAME": "v_joints", "INTERPOLATION": "flat" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "uvec4", "NAME": "v_joints", "INTERPOLATION": "flat" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" },
  "INPUTS": []
}*/
void main()
{
    bool ok = v_joints == uvec4(3u, 0u, 0u, 7u);
    isf_FragColor = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
