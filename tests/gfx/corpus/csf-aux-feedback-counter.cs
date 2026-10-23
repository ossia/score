/*{
  "DESCRIPTION": "Geometry owner with a read_write AUXILIARY counter: thread 0 adds 1 to geo_state.frame every frame. Wired back onto its own geometry input (a feedback receiver), the counter must read 1, 2, 3, ... frame after frame; a hidden twin swapped every frame made it 1, 1, 2, 2, ... (shader board C45). Used by GfxCsfAuxFeedbackCounter.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-AUXILIARY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ],
      "AUXILIARY": [
        {
          "NAME": "state",
          "ACCESS": "read_write",
          "LAYOUT": [
            { "NAME": "frame", "TYPE": "uint" },
            { "NAME": "pad", "TYPE": "uint[3]" }
          ]
        }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [3, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(3.0, -1.0);
    else if(idx == 2u) p = vec2(-1.0, 3.0);
    ISF_WRITE(geo, position)[idx] = vec4(p, 0.0, 1.0);
    ISF_WRITE(geo, color)[idx] = ISF_READ(geo, color)[idx];
    if(idx == 0u)
        geo_state.frame = geo_state.frame + 1u;
}
