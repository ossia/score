/*{
  "DESCRIPTION": "RGBA32F image output at the render size (agent L2 inlet format tests).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-IMAGE-FORMAT"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba32f", "WIDTH": "$WIDTH", "HEIGHT": "$HEIGHT" }
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

    vec2 uv = (vec2(pos) + 0.5) / vec2(size);

    // HDR values: some channels exceed 1.0 to test 16-bit float range
    float r = uv.x * 2.0;
    float g = uv.y * 2.0;
    float b = sin(TIME + uv.x * 10.0) * 0.5 + 0.5;

    IMG_STORE(outputImage, pos, vec4(r, g, b, 1.0));
}
