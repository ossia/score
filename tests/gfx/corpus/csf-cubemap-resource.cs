/*{
  "DESCRIPTION": "A compute shader reading a TYPE cubemap RESOURCE with IMG_CUBE_LOD: the output's 3x2 cells are the +X, -X, +Y, -Y, +Z, -Z directions of the cube.",
  "ISFVSN": "2", "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "cube", "TYPE": "cubemap" },
    { "NAME": "result", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": 96, "HEIGHT": 64 }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "result" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  int cell = (p.y / 32) * 3 + p.x / 32;
  const vec3 dirs[6] = vec3[6](
      vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0), vec3(0, -1, 0), vec3(0, 0, 1), vec3(0, 0, -1));
  IMG_STORE(result, p, IMG_CUBE_LOD(cube, dirs[cell], 0.0));
}
