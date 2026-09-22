/*{
  "DESCRIPTION": "Guide example 6 -- frame-to-frame feedback, the basis of every particle system. A geometry attribute declared read_write persists across frames, so a pass can read what it wrote last frame and integrate. ISF_READ(geo, attr) and ISF_WRITE(geo, attr) are the readable spelling of geo_attr_in / geo_attr_out; a read_write attribute is always two symbols. Alpha doubles as an initialised flag, because the buffer's first-frame contents are not guaranteed. Pair with guide-rawraster-geo to see it: the triangle brightens the longer it runs.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["GUIDE"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= uint(ISF_WRITE(geo, position).length()))
        return;

    vec2 corners[3] = vec2[3](
        vec2(-0.6, -0.5),
        vec2( 0.6, -0.5),
        vec2( 0.0,  0.6)
    );

    vec4 prev = ISF_READ(geo, color)[idx];

    // First frame: the buffer's contents are undefined, so seed rather than
    // integrate. Alpha is the flag -- a seeded vertex always has alpha 1.
    if(prev.a < 0.5)
    {
        ISF_WRITE(geo, position)[idx] = vec4(corners[idx], 0.5, 1.0);
        ISF_WRITE(geo, color)[idx]    = vec4(0.02, 0.0, 0.0, 1.0);
        return;
    }

    // Integrate: accumulate red, saturating well below 1.0 so a longer run is
    // always distinguishable from a shorter one.
    ISF_WRITE(geo, position)[idx] = vec4(corners[idx], 0.5, 1.0);
    ISF_WRITE(geo, color)[idx]    = vec4(min(prev.r + 0.03, 0.95), 0.0, 0.0, 1.0);
}
