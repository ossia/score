/*{
  "DESCRIPTION": "Compute-written vertical green ramp: green = gl_GlobalInvocationID.y / (height-1), with constant red 0.25 / blue 0.75 as garbage detectors. A storage-image texel index is top-down, so row 0 (green 0.0) is the TOP of the delivered image and the last row (green 1.0) is the BOTTOM -- the opposite vertical direction from isf-gradient-y.fs, which is the ISF bottom-left convention.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-ORIENTATION"],
  "RESOURCES": [
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

    float v = float(pos.y) / float(size.y - 1);
    IMG_STORE(outputImage, pos, vec4(0.25, v, 0.75, 1.0));
}
