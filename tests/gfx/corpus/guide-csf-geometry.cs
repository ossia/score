/*{
  "DESCRIPTION": "Guide example 5 -- a compute shader that PRODUCES geometry for a rasteriser downstream. A geometry RESOURCE declares its attributes by SEMANTIC; the parser then exposes each one as <name>_out for writing and <name>_in for reading, so a read_write attribute is two symbols, not one. EXECUTION_MODEL PER_VERTEX dispatches one invocation per vertex of TARGET. Pair with guide-rawraster-geo, whose VERTEX_INPUTS match these semantics.",
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
    if(idx >= uint(geo_position_out.length()))
        return;

    // A triangle in the z=0 plane, wound counter-clockwise.
    vec2 corners[3] = vec2[3](
        vec2(-0.6, -0.5),
        vec2( 0.6, -0.5),
        vec2( 0.0,  0.6)
    );
    vec4 colors[3] = vec4[3](
        vec4(1.0, 0.0, 0.0, 1.0),
        vec4(0.0, 1.0, 0.0, 1.0),
        vec4(0.0, 0.0, 1.0, 1.0)
    );

    geo_position_out[idx] = vec4(corners[idx], 0.5, 1.0);
    geo_color_out[idx]    = colors[idx];
}
