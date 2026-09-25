/*{
  "DESCRIPTION": "Adds 0.1 to the upstream `state` auxiliary at its own index and writes the result as colour. Without PERSISTENT the auxiliary is a copy refreshed from the upstream every frame, so the colour stays at upstream + 0.1. Used by GfxCsfReadWriteCopyA5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "color", "SEMANTIC": "color", "TYPE": "vec4", "ACCESS": "write_only" }
      ],
      "AUXILIARY": [
        { "NAME": "state", "ACCESS": "read_write", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [3, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 3u || idx >= uint(geo_state.data.length()))
        return;
    vec4 s = geo_state.data[idx] + vec4(0.1);
    geo_state.data[idx] = s;
    geo_color_out[idx] = vec4(s.rgb, 1.0);
}
