/*{
  "DESCRIPTION": "Tests geometry VERTEX_COUNT as $variable expression. Pipeline: this -> raw-raster-basic.fs -> Window. Slider 'numPoints' (8-512) controls particle count. If expression vertex count works: particle count follows the slider. If broken: fixed count or zero.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY"],
  "RESOURCES": [
    { "NAME": "numPoints", "TYPE": "long", "DEFAULT": 64, "MIN": 8, "MAX": 512 },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$numPoints",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
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
    uint count = uint(geo_position_out.length());
    if(idx >= count)
        return;

    float t = float(idx) / float(max(count - 1u, 1u));
    float angle = t * 6.28318 * 2.0;
    float radius = 1.0 + t * 2.0;

    geo_position_out[idx] = vec4(
        cos(angle) * radius,
        sin(angle) * radius,
        0.0, 1.0
    );

    geo_color_out[idx] = vec4(t, 0.5, 1.0 - t, 1.0);
}
