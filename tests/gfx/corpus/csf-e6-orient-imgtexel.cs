/*{
  "DESCRIPTION": "Compute copy of its input through IMG_TEXEL and IMG_STORE at the same invocation index. Used by GfxE6ComputeImageOrientation.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-ORIENTATION"],
  "RESOURCES": [
    { "NAME": "inputTex", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;

    IMG_STORE(outputImage, pos, IMG_TEXEL(inputTex, pos));
}
