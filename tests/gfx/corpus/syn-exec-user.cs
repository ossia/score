/*{
  "DESCRIPTION": "EXECUTION_MODEL USER: the runtime creates three integer ports (X, Y, Z) for the dispatch counts and defaults each to 1. With LOCAL_SIZE [4,1,1] that is exactly 4 invocations, so the tally reads 4 -- distinguishing a USER pass that was wired up and dispatched from one silently treated as 2D_IMAGE (which would size from an image and count differently) or never dispatched at all.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-EXECUTION-MODEL"],
  "RESOURCES": [
    {
      "NAME": "tally",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [ { "NAME": "hits", "TYPE": "int[64]" } ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [4, 1, 1], "EXECUTION_MODEL": { "TYPE": "USER" } },
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  // Unconditional: records which PASSINDEX values actually reached the shader,
  // so a USER pass whose index is not 0 is separable from one never dispatched.
  if(PASSINDEX >= 0 && PASSINDEX < 8)
    tally.hits[56 + PASSINDEX] = 9;

  if(PASSINDEX == 0)
  {
    uint idx = gl_GlobalInvocationID.x;
    if(idx < 64u)
      tally.hits[idx] = 1;
    // Marker slot: proves the pass ran at all, independent of how many
    // invocations it got.
    tally.hits[63] = 7;
    return;
  }

  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(outputImage);
  if(pos.x >= size.x || pos.y >= size.y)
    return;

  // Slots 56..62 hold the per-PASSINDEX markers, so the invocation tally stops
  // short of them: summing those too would report dispatch counts that no
  // dispatch produced.
  int total = 0;
  for(int i = 0; i < 56; i++)
    total += tally.hits[i];
  // Blue carries the marker so "pass never ran" and "pass ran with no
  // invocations counted" are separable.
  float marker = float(tally.hits[63]) / 255.0;

  IMG_STORE(outputImage, pos, vec4(float(total) / 255.0, 0.0, marker, 1.0));
}
