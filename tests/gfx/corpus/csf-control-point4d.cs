/*{
  "DESCRIPTION": "Control-value coverage for a compute shader: a TYPE:float RESOURCE followed by a TYPE:point4D RESOURCE, so the vec4 starts on the next 16-byte boundary of the generated std140 Params block. Left half of the written image: (p4.x, p4.y, p4.z, 1); right half: (p4.w, level, 0, 1). With the DEFAULTs the readback is (51, 102, 153, 255) left and (204, 255, 0, 255) right.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "RESOURCES": [
    { "NAME": "level", "TYPE": "float", "DEFAULT": 1.0 },
    { "NAME": "p4", "TYPE": "point4D", "DEFAULT": [0.2, 0.4, 0.6, 0.8] },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;

    if(pos.x < size.x / 2)
        IMG_STORE(outputImage, pos, vec4(p4.x, p4.y, p4.z, 1.0));
    else
        IMG_STORE(outputImage, pos, vec4(p4.w, level, 0.0, 1.0));
}
