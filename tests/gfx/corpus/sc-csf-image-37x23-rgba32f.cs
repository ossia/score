/*{
  "DESCRIPTION": "37x23 RGBA32F image output (agent SC single-cable inlet tests).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-IMAGE-FORMAT"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba32f", "WIDTH": "37", "HEIGHT": "23" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [16, 16, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;
    IMG_STORE(outputImage, pos, vec4(2.0, 0.5, 0.25, 1.0));
}
