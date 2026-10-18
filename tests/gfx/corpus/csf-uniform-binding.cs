/*{
  "DESCRIPTION": "A top-level uniform block, then a sampled texture, then a write-only output image that copies the texture. With nothing cabled to the block it reads zeros; the texture must still arrive at its own binding. Used by GfxCsfUniformBinding.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "params", "TYPE": "uniform", "LAYOUT": [ { "NAME": "tint", "TYPE": "vec4" } ] },
    { "NAME": "after", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 64 || pos.y >= 64)
        return;
    vec2 uv = (vec2(pos) + 0.5) / 64.0;
    imageStore(outputImage, pos, texture(after, uv) + params.tint);
}
