/*{
  "DESCRIPTION": "Same vertical green ramp as csf-gradient-y.cs, written through IMG_STORE instead of a bare imageStore. The author's model is top-down: gl_GlobalInvocationID.y == 0 is the TOP row, so green runs 0 at the top to 1 at the bottom, and the delivered image must read that way on every backend. csf-gradient-y.cs stores at the raw texel index and is therefore upside down on OpenGL, where the index and the render target are mirrored against each other; the macro is what makes this one portable.",
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
