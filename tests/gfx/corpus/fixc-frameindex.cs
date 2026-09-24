/*{
  "DESCRIPTION": "Stores FRAMEINDEX / 255 in red.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
    IMG_STORE(outputImage, p, vec4(float(FRAMEINDEX) / 255.0, 0.0, 0.0, 1.0));
}
