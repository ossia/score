/*{
  "DESCRIPTION": "A 64^3 R32_UINT voxel grid over the unit cube [-0.5, 0.5]^3, in the layout voxelize_scene_aabb writes (cell = floor((p + 0.5) * 64)): bit 0 above y = 0.15 (red in probe_voxel_orbit_view), bit 1 where x > 0.15 below it (green), bit 2 where x < -0.15 below it (blue), empty in between. Feeds probe_voxel_orbit_view.csf in GfxLibraryPresetFixesO.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-3D"],
  "RESOURCES": [
    { "NAME": "grid", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "r32ui", "WIDTH": "64", "HEIGHT": "64", "DEPTH": "64" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [4, 4, 4], "EXECUTION_MODEL": { "TYPE": "3D_IMAGE", "TARGET": "grid" } }
  ]
}*/

void main()
{
    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 size = imageSize(grid);
    if(any(greaterThanEqual(pos, size)))
        return;

    vec3 p = (vec3(pos) + 0.5) / vec3(size) - 0.5;
    uint bits = 0u;
    if(p.y > 0.15)
        bits = 1u;
    else if(p.x > 0.15)
        bits = 2u;
    else if(p.x < -0.15)
        bits = 4u;
    imageStore(grid, pos, uvec4(bits));
}
