/*{
  "DESCRIPTION": "Tests geometry AUXILIARY buffers (structured SSBOs that travel with geometry). Pipeline: this -> raw-raster-auxiliary.fs -> Window (tests aux end-to-end) or this -> raw-raster-basic.fs -> Window (tests just the geometry). Has a 'stats' auxiliary with frameCount/avgX/avgY. Pass 0: updates stats. Pass 1: uses stats to color particles. If auxiliary works: particles shift color as frameCount increases. If broken: static color.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-AUXILIARY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "128",
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
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } }
  ]
}*/

void main()
{
    if(PASSINDEX == 0)
    {
        // Single-thread pass: update stats
        geo_stats.frameCount = geo_stats.frameCount + 1u;
        float t = float(geo_stats.frameCount) * 0.01;
        geo_stats.avgX = sin(t) * 0.5;
        geo_stats.avgY = cos(t) * 0.5;
    }
    else
    {
        // Per-particle pass: use stats to position and color
        uint idx = gl_GlobalInvocationID.x;
        uint count = uint(geo_position_out.length());
        if(idx >= count)
            return;

        float t = float(idx) / float(count - 1u);
        float angle = t * 6.28318;

        float cx = geo_stats.avgX;
        float cy = geo_stats.avgY;

        geo_position_out[idx] = vec4(
            cx + cos(angle) * 2.0,
            cy + sin(angle) * 2.0,
            0.0, 1.0
        );

        // Color cycles with frameCount
        float hue = fract(float(geo_stats.frameCount) / 120.0);
        geo_color_out[idx] = vec4(
            0.5 + 0.5 * sin(hue * 6.28 + t * 3.0),
            0.5 + 0.5 * sin(hue * 6.28 + 2.09 + t * 3.0),
            0.5 + 0.5 * sin(hue * 6.28 + 4.19 + t * 3.0),
            1.0
        );
    }
}
