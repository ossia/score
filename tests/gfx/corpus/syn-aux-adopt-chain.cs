/*{
  "DESCRIPTION": "Consumer half of the auxiliary-adoption lifetime fixture: takes geometry in, declares the SAME 'stats' auxiliary as its producer, and does not own the geometry (no VERTEX_COUNT). Chained producer -> this -> this, the middle node first allocates its own 'stats' buffer, the last node adopts that buffer, and then the middle node adopts its own upstream's and releases the one the last node is still holding. That is the shape the corpus crashes on: the binder is a different node than the releaser, its aux.buffer keeps the same address, so the binding hash never changes and its shader resource bindings are never rebuilt.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-AUXILIARY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ],
      "AUXILIARY": [
        {
          "NAME": "stats",
          "ACCESS": "read_write",
          "LAYOUT": [
            { "NAME": "frameCount", "TYPE": "uint" },
            { "NAME": "avgX", "TYPE": "float" },
            { "NAME": "avgY", "TYPE": "float" }
          ]
        }
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
    if(idx >= uint(ISF_READ(geo, color).length()))
        return;

    // Touch the auxiliary so the binding is live, and the geometry so the
    // chain has something to carry.
    float t = float(geo_stats.frameCount) * 0.01;
    vec4 c = ISF_READ(geo, color)[idx];
    ISF_WRITE(geo, color)[idx] = vec4(clamp(c.rgb + vec3(0.002 + t * 0.0), 0.0, 1.0), 1.0);
}
