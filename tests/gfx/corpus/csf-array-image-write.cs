/*{
  "DESCRIPTION": "Writes a write_only image2DArray storage image (4 layers): a 2D dispatch runs once per (x,y) texel and loops over all 4 layers, writing a distinct non-black tint per layer. Downstream a sampler2DArray reader (csf-array-image-read.fs) inspects several layers. On Qt-GL before the cube/array layered-storage workaround, only layer 0 is written and every other layer reads back black; on Vulkan all four are always correct.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-ARRAY"],
  "RESOURCES": [
    { "NAME": "layers", "TYPE": "image", "ACCESS": "write_only", "IS_ARRAY": true, "LAYERS": "4", "FORMAT": "rgba8", "WIDTH": "32", "HEIGHT": "32" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "layers" } }
  ]
}*/

void main()
{
    ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
    ivec3 size = imageSize(layers);
    if(any(greaterThanEqual(xy, size.xy)))
        return;

    // Every layer gets a bright, non-black, per-layer-distinct tint so a
    // downstream sampler can tell that layers past 0 were written and differ.
    for(int layer = 0; layer < 4; layer++)
    {
        vec3 tint = vec3(0.25) + 0.6 * vec3(
            float(layer == 1 || layer == 3),
            float(layer == 2 || layer == 3),
            float(layer == 0));
        imageStore(layers, ivec3(xy, layer), vec4(tint, 1.0));
    }
}
