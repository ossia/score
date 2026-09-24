/*{
  "DESCRIPTION": "A CSF input declared TYPE image with no ACCESS or FORMAT (the fragment-ISF spelling) is a sampled texture: each texel of the 64x64 output copies the input at the same normalised coordinate.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ],
  "RESOURCES": [ { "NAME": "outImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" } ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outImage" } } ]
}*/
void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(pos, imageSize(outImage)))) return;
    IMG_STORE(outImage, pos, IMG_NORM_PIXEL(inputImage, (vec2(pos) + 0.5) / 64.0));
}
