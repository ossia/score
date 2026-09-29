/*{
  "DESCRIPTION": "Paints the size of its output image, which has no WIDTH / HEIGHT and so is allocated at the render size, in pixels out of 255: R = width, G = height.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8" }
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
    imageStore(outputImage, pos, vec4(vec2(size) / 255.0, 0.0, 1.0));
}
