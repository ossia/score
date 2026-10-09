/*{
  "DESCRIPTION": "Writes an image of $size x $size whose pixels hold its own width / 255 in red. Used by GfxSizeExpressionInput.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [ { "NAME": "size", "TYPE": "long", "DEFAULT": 16, "MIN": 1, "MAX": 64 } ],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "$size", "HEIGHT": "$size" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [16, 16, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/
void main()
{
  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  ivec2 sz = imageSize(outputImage);
  if(pos.x >= sz.x || pos.y >= sz.y)
    return;
  IMG_STORE(outputImage, pos, vec4(float(sz.x) / 255.0, 0.0, 0.0, 1.0));
}
