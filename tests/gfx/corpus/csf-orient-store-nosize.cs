/*{
  "DESCRIPTION": "csf-orient-store.cs with every size query removed from the body: the dispatch is exactly one invocation per texel, so no bounds check is needed and the constant 63.0 replaces imageSize().y - 1. What is left is a shader whose ONLY possible source of an image-size query is ISF_STORE_COORD, which is what makes the compute storage-image gate readable off the baked code on a machine that cannot run the backend. Same picture as csf-orient-store.cs: green ramps 0 at the top to 1 at the bottom.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-ORIENTATION"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    IMG_STORE(outputImage, pos, vec4(0.25, float(pos.y) / 63.0, 0.75, 1.0));
}
