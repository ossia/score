/*{
  "DESCRIPTION": "Tests CSF image resource with RGBA16F format and expression-based dimensions. Image width/height use '$WIDTH' and '$HEIGHT' to match the render output size. If expression dimensions work: output fills the screen. If expressions fail: image may be wrong size or zero.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-IMAGE-FORMAT"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba16f", "WIDTH": "$WIDTH", "HEIGHT": "$HEIGHT" }
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

    imageStore(outputImage, pos, vec4(r, g, b, 1.0));
}
