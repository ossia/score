/*{
  "DESCRIPTION": "Paints the size of the storage-image AUXILIARY of its geometry input, which has no WIDTH / HEIGHT and so is allocated at the render size, in pixels out of 255: R = width, G = height. The output image is at the render size too.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "foo", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false } ],
      "AUXILIARY": [ { "NAME": "scratch", "TYPE": "storage_image", "ACCESS": "read_write", "FORMAT": "rgba8" } ] }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;
    vec4 v = imageLoad(scratch, ivec2(0));
    imageStore(outputImage, pos, vec4(vec2(imageSize(scratch)) / 255.0, 0.0, 1.0) + 0.0 * v);
}
