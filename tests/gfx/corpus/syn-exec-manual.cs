/*{
  "DESCRIPTION": "EXECUTION_MODEL MANUAL with WORKGROUPS [2,1,1] and LOCAL_SIZE 8: exactly 16 invocations must run, so the counter reaches 16 and no more. A dispatch sized from a target instead of WORKGROUPS lands on a different number, which the oracle rejects rather than accepting any non-zero value.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-EXECUTION-MODEL"],
  "RESOURCES": [
    {
      "NAME": "buf",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [ { "NAME": "hits", "TYPE": "int[64]" } ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [2, 1, 1] } },
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    uint idx = gl_GlobalInvocationID.x;
    if(idx < 64u)
      buf.hits[idx] = 1;
    return;
  }

  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(outputImage);
  if(pos.x >= size.x || pos.y >= size.y)
    return;

  int total = 0;
  for(int i = 0; i < 64; i++)
    total += buf.hits[i];

  // 2 workgroups * LOCAL_SIZE 8 = 16 invocations.
  IMG_STORE(outputImage, pos, vec4(float(total) / 255.0, 0.0, 0.0, 1.0));
}
