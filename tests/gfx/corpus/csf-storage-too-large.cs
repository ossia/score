/*{
  "DESCRIPTION": "A write-only vec4[] storage buffer sized by its control, and an 8x8 output image. The pass paints the image green when the bound buffer holds the 536870911 elements the test sets its control to, red when it holds fewer. It writes nothing to the buffer. Used by GfxCsfStorageTooLarge.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "buf", "TYPE": "storage", "ACCESS": "write_only", "LAYOUT": [ { "NAME": "v", "TYPE": "vec4[]" } ] },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
  // GfxCsfStorageTooLarge sets the size control to 536870911 elements: 8 GiB.
  const bool whole = buf.v.length() >= 536870911;
  imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy),
             whole ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0));
}
