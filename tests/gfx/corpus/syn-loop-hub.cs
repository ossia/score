/*{
  "DESCRIPTION": "Loop owner for the feedback regression test: owns the geometry (VERTEX_COUNT), lays out the 16x11 half-cell grid, passes the fed-back value straight through; the buffers start zeroed so the loop accumulates from black. This pass-through IS the frame transition -- it only works when the owner binds _in and _out to different halves of its pair, which is why the owner must never adopt the shared upstream handle.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY"],
  "RESOURCES": [
    { "NAME": "numPoints", "TYPE": "long", "DEFAULT": 192, "MIN": 24, "MAX": 528 },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$numPoints",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    uint count = uint(ISF_WRITE(geo, position).length());
    if(idx >= count)
        return;

    uint k = idx / 3u;
    uint corner = idx % 3u;
    float cx = -1.0 + float(k % 16u) * 0.125;
    float cy = -1.0 + float((k / 16u) % 11u) * (2.0 / 11.0);
    vec2 p = vec2(cx, cy);
    if(corner == 1u)      p.x += 0.125;
    else if(corner == 2u) p.y += 2.0 / 11.0;
    ISF_WRITE(geo, position)[idx] = vec4(p, 0.0, 1.0);

    // No seeding at all: the buffers start zeroed, so the loop accumulates from
    // black and the test needs no frame counter and no liveness flag, either of
    // which would make it depend on something other than the property under
    // test. The pass-through IS the frame transition -- it only advances when
    // the owner binds _in and _out to different halves of its pair.
    ISF_WRITE(geo, color)[idx] = ISF_READ(geo, color)[idx];
}
