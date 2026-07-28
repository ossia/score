/*{
  "DESCRIPTION": "Tests 3D_IMAGE dispatch model and 3D image write. Creates a 64x64x64 RGBA8 volume with a pattern that varies along all 3 axes. Connect to 3d-slice-viewer.fs to inspect slices. If 3D image write and dispatch work: slices show varying patterns at each Z. If broken: all slices identical or black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-3D"],
  "RESOURCES": [
    { "NAME": "volume", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "64", "HEIGHT": "64", "DEPTH": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [4, 4, 4], "EXECUTION_MODEL": { "TYPE": "3D_IMAGE" } }
  ]
}*/

void main()
{
    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 size = imageSize(volume);
    if(any(greaterThanEqual(pos, size)))
        return;

    vec3 uv = (vec3(pos) + 0.5) / vec3(size);

    // Each axis contributes a different color channel
    // X -> red stripes
    float r = 0.5 + 0.5 * sin(uv.x * 20.0);
    // Y -> green stripes (different frequency)
    float g = 0.5 + 0.5 * sin(uv.y * 15.0);
    // Z -> blue gradient (so each slice is a different shade)
    float b = uv.z;

    // Add time-based animation to prove it updates
    r *= 0.5 + 0.5 * sin(TIME + uv.z * 6.28);

    imageStore(volume, pos, vec4(r, g, b, 1.0));
}
