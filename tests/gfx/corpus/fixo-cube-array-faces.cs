/*{
  "DESCRIPTION": "A cube as a 6-layer image2DArray (the IBL precompute layout: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z), each layer a flat per-face tint: bits (face&1, face>>1&1, face>>2&1) scaled to 0.25 + 0.6*bit. +Y is (0.25, 0.85, 0.25), -Y (0.85, 0.85, 0.25). Feeds cubemap_array_orbit.frag in GfxLibraryPresetFixesO.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-ARRAY"],
  "RESOURCES": [
    { "NAME": "faces", "TYPE": "image", "ACCESS": "write_only", "IS_ARRAY": true, "LAYERS": "6", "FORMAT": "rgba8", "WIDTH": "32", "HEIGHT": "32" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "faces" } }
  ]
}*/

void main()
{
    ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
    ivec3 size = imageSize(faces);
    if(any(greaterThanEqual(xy, size.xy)))
        return;

    for(int face = 0; face < 6; face++)
    {
        vec3 bits = vec3(float(face & 1), float((face >> 1) & 1), float((face >> 2) & 1));
        IMG_STORE_LAYER(faces, ivec3(xy, face), vec4(vec3(0.25) + 0.6 * bits, 1.0));
    }
}
