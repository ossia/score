/*{
  "DESCRIPTION": "A gather that reads the FAR end of its own attribute. position is the 16x11 half-cell grid of syn-geo-count-user.cs so every legal count rasterizes; tint is `gather` and each invocation reads the mirrored index N-1-idx. The seed puts red in the FIRST triangle only, so after one mirror step the red lands in the LAST triangle -- which can only happen if _in covers the whole buffer. A truncated _in (the 8-byte snapshot of ledger 9.97) returns zero for every index past the first and the red never appears anywhere.",
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
        { "NAME": "tint",     "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "gather" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } }
  ]
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
    if(corner == 1u)
        p.x += 0.125;
    else if(corner == 2u)
        p.y += 2.0 / 11.0;
    ISF_WRITE(geo, position)[idx] = vec4(p, 0.0, 1.0);

    uint N = (count / 3u) * 3u;
    if(N == 0u || idx >= N)
        return;

    vec4 prev = ISF_READ(geo, tint)[idx];
    if(prev.a < 0.5)
    {
        // Seed: the FIRST triangle red, every other one blue.
        ISF_WRITE(geo, tint)[idx]
            = (k == 0u) ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
        return;
    }

    // Mirror: the value at the far end of the buffer. Reaching it at all
    // requires _in to span the whole allocation.
    uint mirror = (N - 1u - idx);
    mirror = (mirror / 3u) * 3u + corner;
    ISF_WRITE(geo, tint)[idx] = ISF_READ(geo, tint)[mirror];
}
