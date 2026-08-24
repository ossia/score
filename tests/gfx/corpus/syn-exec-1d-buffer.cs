/*{
  "DESCRIPTION": "EXECUTION_MODEL 1D_BUFFER across a 64-entry storage buffer: invocation i writes i*2. A second 2D_IMAGE pass paints entry 32, so the readback is 64/255 red only when the 1D dispatch covered the whole range; a dispatch that never ran, or covered one workgroup, leaves it at zero.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-EXECUTION-MODEL"],
  "RESOURCES": [
    {
      "NAME": "buf",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [ { "NAME": "vals", "TYPE": "int[64]" } ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER", "TARGET": "buf" } },
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    uint idx = gl_GlobalInvocationID.x;
    if(idx < 64u)
      buf.vals[idx] = int(idx) * 2;
    return;
  }

  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(outputImage);
  if(pos.x >= size.x || pos.y >= size.y)
    return;

  float v = float(buf.vals[32]) / 255.0;
  IMG_STORE(outputImage, pos, vec4(v, 0.0, 0.0, 1.0));
}
