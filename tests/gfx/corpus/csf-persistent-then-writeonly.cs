/*{
  "DESCRIPTION": "A write-only image declared after a PERSISTENT one: 'state' is written red, 'result' green. The 'result' outlet must show green.",
  "ISFVSN": "2", "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "state", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "PERSISTENT": true, "WIDTH": 16, "HEIGHT": 16 },
    { "NAME": "result", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": 16, "HEIGHT": 16 }
  ],
  "PASSES": [ { "LOCAL_SIZE": [16, 16, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "result" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  IMG_STORE(state, p, vec4(1.0, 0.0, 0.0, 1.0));
  IMG_STORE(result, p, vec4(0.0, 1.0, 0.0, 1.0));
}
