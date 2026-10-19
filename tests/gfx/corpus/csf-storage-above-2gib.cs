/*{
  "DESCRIPTION": "A read_write vec4[] storage buffer sized by its control, and an 8x8 output image. GfxCsfStorageTooLarge sets the control to 150000000 elements (2.4 GB, above INT_MAX bytes). Pass 0 writes a marker to the last element, pass 1 paints the image green when it reads the marker back, red otherwise. It does not use length(): NVIDIA's Vulkan driver (595.99) returns 0 for buffers above 2 GiB.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "buf", "TYPE": "storage", "ACCESS": "read_write", "LAYOUT": [ { "NAME": "v", "TYPE": "vec4[]" } ] },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  // Not a constant: NVIDIA's GL compiler exhausts memory on a constant index
  // this large into an unsized array, even though the pass never runs there.
  const uint last = 149999999u + uint(min(FRAMEINDEX, 0));
  // The buffer is created at its default 1024 elements before the control
  // applies: never write past those.
  if(uint(buf.v.length()) == 1024u)
    return;
  const vec4 marker = vec4(0.25, 0.5, 0.75, 1.0);
  if(PASSINDEX == 0)
  {
    buf.v[last] = marker;
    return;
  }
  imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy),
             buf.v[last] == marker ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0));
}
