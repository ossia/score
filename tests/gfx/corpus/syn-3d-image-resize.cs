/*{
  "DESCRIPTION": "Control-driven 3D storage image size, for the runtime-resize test (P1-21). A single long control 'edge' drives WIDTH/HEIGHT/DEPTH of a cubic rgba8 volume; the compute pass writes a closed form that ENCODES the LIVE allocation size: byte(R) == imageSize(volume).x exactly, G is a linear ramp along y, B a linear ramp along z (so 3d-slice-viewer.fs at sliceZ=0.5 reads B ~ 128). Sampling one slice therefore distinguishes 'reallocated and rewritten at the new edge' (R == new edge) from 'stale allocation kept from the previous edge' (R == old edge). Follows csf-3d-image-write.cs conventions (3D_IMAGE dispatch model, guard on imageSize, LOCAL_SIZE 4x4x4) but is deterministic: no TIME term.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-3D"],
  "INPUTS": [
    { "NAME": "edge", "TYPE": "long", "DEFAULT": 64, "MIN": 1, "MAX": 256 }
  ],
  "RESOURCES": [
    { "NAME": "volume", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "$edge", "HEIGHT": "$edge", "DEPTH": "$edge" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [4, 4, 4], "EXECUTION_MODEL": { "TYPE": "3D_IMAGE" } }
  ]
}*/

void main()
{
    ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 sz = imageSize(volume);
    if(any(greaterThanEqual(pos, sz)))
        return;

    vec3 uvw = (vec3(pos) + 0.5) / vec3(sz);

    // R encodes the LIVE allocation edge. sz.x/255 is exactly representable
    // in UNORM8, so the readback byte equals sz.x with no rounding slack:
    // 64^3 -> R=64, 32^3 -> R=32, 96^3 -> R=96. The dispatch is sized from
    // the actual texture (RenderedCSFNode.cpp:4511-4521), so R always tells
    // which allocation the writer really covered -- the size fingerprint.
    float r = float(sz.x) / 255.0;

    // G: linear ramp along y. Linear in uvw.y, so a bilinear sampler
    // reconstructs G ~ 255*uv.y on ANY edge -- shape check, edge-independent.
    float g = uvw.y;

    // B: linear ramp along z; the slice viewer at sliceZ = 0.5 reads ~128
    // (same closed form as csf-3d-image-write.cs's blue gradient).
    float b = uvw.z;

    imageStore(volume, pos, vec4(r, g, b, 1.0));
}
