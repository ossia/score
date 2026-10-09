/*{
  "DESCRIPTION": "PERSISTENT storage: each frame stores counter_prev + 1 into counter, and paints the output with counter_prev / 255 in red. Used by GfxCsfStoragePersistent.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-STORAGE"],
  "RESOURCES": [
    {
      "NAME": "counter",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "PERSISTENT": true,
      "LAYOUT": [ { "NAME": "frames", "TYPE": "uint" } ]
    },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [16, 16, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 16 || pos.y >= 16)
        return;

    const uint seen = counter_prev.frames;
    if(pos == ivec2(0))
        counter.frames = seen + 1u;
    IMG_STORE(outputImage, pos, vec4(float(seen) / 255.0, 0.0, 0.0, 1.0));
}
