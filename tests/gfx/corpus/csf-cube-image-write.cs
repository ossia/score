/*{
  "DESCRIPTION": "Writes a write_only imageCube storage image: a 2D dispatch runs once per (x,y) texel and loops over all 6 cube faces, writing a distinct non-black tint per face. Downstream a samplerCube reader (csf-cube-image-read.fs) inspects several faces. On Qt-GL before the cube/array layered-storage workaround, only face 0 (+X) is written and every other face reads back black; on Vulkan all six are always correct.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-CUBEMAP"],
  "RESOURCES": [
    { "NAME": "probe", "TYPE": "image_cube", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "32", "HEIGHT": "32" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "probe" } }
  ]
}*/

void main()
{
    ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(probe).xy;
    if(any(greaterThanEqual(xy, size)))
        return;

    // Vulkan cube face ordering: 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z.
    // Every face gets a bright, non-black, per-face-distinct tint so a downstream
    // sampler can tell (a) that faces past 0 were written and (b) that they differ.
    for(int face = 0; face < 6; face++)
    {
        vec3 bits = vec3(float(face & 1), float((face >> 1) & 1), float((face >> 2) & 1));
        vec3 tint = vec3(0.25) + 0.6 * bits;
        imageStore(probe, ivec3(xy, face), vec4(tint, 1.0));
    }
}
