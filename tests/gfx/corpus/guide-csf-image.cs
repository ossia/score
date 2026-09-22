/*{
  "DESCRIPTION": "Guide example 4 -- a compute shader writing a storage image. Note the two conventions that differ from ISF: a storage-image texel index is TOP-DOWN (row 0 is the top of the delivered image, the opposite of isf_FragNormCoord), and every write goes through IMG_STORE rather than imageStore, so the same source bakes on every backend. The bounds guard is mandatory: the dispatch is rounded up to whole workgroups, so the last one runs past the image.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["GUIDE"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos  = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;

    float u = float(pos.x) / float(size.x - 1);
    IMG_STORE(outputImage, pos, vec4(u, 0.25, 1.0 - u, 1.0));
}
