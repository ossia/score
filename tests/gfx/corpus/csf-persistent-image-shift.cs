/*{
  "DESCRIPTION": "A PERSISTENT 8x1 read_write image shifted right by one texel per frame: texel 0 is lit, texel x copies texel x - 1 of the previous frame. One invocation walks the row from left to right, so it reads state_prev(x - 1) after writing state(x - 1). With a separate, zero-filled history, frame k lights texels 0 .. k-1; reading the image being written lights the whole row at once. Used by GfxCsfPersistentImageHistory.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "state", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "1", "PERSISTENT": true }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } }
  ]
}*/

void main()
{
  imageStore(state, ivec2(0, 0), vec4(1.0, 0.0, 0.0, 1.0));
  for(int x = 1; x < 8; x++)
    imageStore(state, ivec2(x, 0), imageLoad(state_prev, ivec2(x - 1, 0)));
}
