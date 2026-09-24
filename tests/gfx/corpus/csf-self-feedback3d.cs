/*{
  "DESCRIPTION": "3D self-feedback counter: volOut = prev + 16/255, with volOut cabled back into prev through a delayed cable.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "INPUTS": [
    { "NAME": "prev", "TYPE": "texture", "DIMENSIONS": 3 }
  ],
  "RESOURCES": [
    { "NAME": "volOut", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "8", "HEIGHT": "8", "DEPTH": "8" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [4, 4, 4], "EXECUTION_MODEL": { "TYPE": "3D_IMAGE", "TARGET": "volOut" } } ]
}*/
void main()
{
    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 size = imageSize(volOut);
    if(any(greaterThanEqual(pos, size))) return;
    vec3 p = (vec3(pos) + 0.5) / vec3(size);
    vec4 pr = textureLod(prev, p, 0.0);
    imageStore(volOut, pos, vec4(pr.r + 16.0 / 255.0, 0.0, 0.0, 1.0));
}
