/*{
  "DESCRIPTION": "Fills the output with the red of its texture input sampled at u = 0.4, v = 0.5. Used by GfxInputSamplerFilterI1 to see the inlet's filter change.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "src", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "32", "HEIGHT": "32" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 32 || pos.y >= 32)
        return;
    float r = textureLod(src, vec2(0.4, 0.5), 0.0).r;
    imageStore(outputImage, pos, vec4(r, 0.0, 0.0, 1.0));
}
