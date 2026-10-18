/*{
  "DESCRIPTION": "Pass 0 paints the 8x8 output red. Pass 1 is a 1D_BUFFER pass over 64 items with LOCAL_SIZE 1024 and STRIDE_X 4194304: workgroup size times stride is 2^32, past int. One workgroup covers it all; the pass writes nothing. Used by GfxCsfDispatchSize.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } },
    { "LOCAL_SIZE": [1024, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER", "TARGET": "64", "STRIDE_X": 4194304 } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
    imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(1.0, 0.0, 0.0, 1.0));
}
