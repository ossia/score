/*{
  "DESCRIPTION": "syn-geo-asym-tri.cs with its geometry declared TOPOLOGY triangles: the same lopsided triangle (19.281 % of the frame), labelled as the triangles it holds instead of the points a compute shader's geometry is labelled by default.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-GEOMETRY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "TOPOLOGY": "triangles",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
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
    if(idx >= 3u)
        return;

    vec2 p = vec2(-0.80, -0.60);
    vec3 c = vec3(1.0, 0.0, 0.0);
    if(idx == 1u) { p = vec2( 0.55, -0.20); c = vec3(0.0, 1.0, 0.0); }
    if(idx == 2u) { p = vec2(-0.10,  0.75); c = vec3(0.0, 0.0, 1.0); }

    geo_position_out[idx] = vec4(p, 0.0, 1.0);
    geo_color_out[idx] = vec4(c, 1.0);
}
