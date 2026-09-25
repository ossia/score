/*{
  "DESCRIPTION": "CSF passthrough declaring ALPHA straight: stores IMG_NORM_PIXEL and IMG_PIXEL of its image input, averaged.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "ALPHA": "straight",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 sz = imageSize(outputImage);
    if (p.x >= sz.x || p.y >= sz.y) return;
    vec2 uv = (vec2(p) + 0.5) / vec2(sz);
    vec4 a = IMG_NORM_PIXEL(inputImage, uv);
    vec4 b = IMG_PIXEL(inputImage, uv * vec2(textureSize(inputImage, 0)));
    IMG_STORE(outputImage, p, (a + b) * 0.5);
}
