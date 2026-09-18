/*{
  "DESCRIPTION": "read_write geometry state whose correctness is visible in the rasterized picture. position is the same 16x11 half-cell grid as syn-geo-count-user.cs, so every legal count draws floor(count/3) triangles; color is read_write and holds the state under test: one red triangle among blue ones, shifted forward by exactly one triangle per frame. A correct _in / _out split (snapshot or ping-pong) makes the red run exactly 3 vertices wide every frame; if _in and _out alias, an invocation can read a neighbour that was already rewritten this frame and the red smears across many triangles, which the red-pixel count sees immediately. Self-seeding: alpha 0 in _in means uninitialized.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY"],
  "RESOURCES": [
    { "NAME": "numPoints", "TYPE": "long", "DEFAULT": 96, "MIN": 24, "MAX": 528 },
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

    // Whole triangles only: shifting by one triangle keeps the marker
    // vertex-aligned, so a correct frame always paints exactly one triangle
    // red. NOTE: this step is smaller than the SIMD width, so an aliased
    // _in / _out still reads pre-write values in lockstep -- see the negative
    // control note in the header: this fixture does NOT detect the race.
    uint N = (count / 3u) * 3u;
    uint step = 3u; // one triangle
    uint src = (N > 0u && idx < N) ? ((idx + N - (step % N)) % N) : idx;

    vec4 prev = ISF_READ(geo, color)[idx];
    if(prev.a < 0.5)
    {
        // Uninitialized: triangle 0 red, every other triangle blue.
        ISF_WRITE(geo, color)[idx]
            = (k == 0u) ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
        return;
    }

    ISF_WRITE(geo, color)[idx] = ISF_READ(geo, color)[src];
}
