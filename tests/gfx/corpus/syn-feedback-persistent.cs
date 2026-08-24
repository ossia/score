/*{
  "DESCRIPTION": "PERSISTENT auxiliary buffer as feedback: each frame increments a counter that survives into the next. Rendered over 5 frames the counter reaches 5, so the readback distinguishes real ping-pong persistence from a buffer cleared or reallocated every frame, which would read 1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-FEEDBACK"],
  "RESOURCES": [
    {
      "NAME": "acc",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "PERSISTENT": true,
      "LAYOUT": [ { "NAME": "frames", "TYPE": "int" } ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    if(gl_GlobalInvocationID.x == 0u)
      acc.frames = acc.frames + 1;
    return;
  }

  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize(outputImage);
  if(pos.x >= size.x || pos.y >= size.y)
    return;

  IMG_STORE(outputImage, pos, vec4(float(acc.frames) / 255.0, 0.0, 0.0, 1.0));
}
