/*{
  "DESCRIPTION": "Compute shader sampling its texture input at u = 1.25 with the WRAP its JSON declares, clamp_to_edge: the right edge (green from isf-left-red-right-green.fs). Repeat, the default, would read u = 0.25 (red). Used by GfxIsfSamplerJson (shader board C22).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SAMPLER"],
  "RESOURCES": [
    { "NAME": "src", "TYPE": "texture", "WRAP": "clamp_to_edge" },
    { "NAME": "outImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "32", "HEIGHT": "32" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(p, imageSize(outImage)))) return;
    IMG_STORE(outImage, p, vec4(IMG_NORM_PIXEL(src, vec2(1.25, 0.5)).rgb, 1.0));
}
