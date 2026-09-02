/*{
  "DESCRIPTION": "Driveable-VERTEX_COUNT geometry producer whose output is GUARANTEED visible at every legal count: every 3 consecutive vertices form one half-cell triangle of a 16x11 grid spanning the viewport, so count c rasterizes floor(c/3) triangles of ~40+ px^2 each. Written for GfxGeometryBufferRebind.cpp after csf-vertex-count-expr.cs's off-viewport spiral proved count-dependent-blank (some counts sample no visible pixel). Color encodes the triangle index so different counts give different pictures.",
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

    // Triangle k occupies grid cell (k % 16, (k / 16) % 11); its three
    // vertices are the cell's lower-left, lower-right and upper-left
    // corners. Cell size 0.125 x (2/11) NDC = 8 x ~11 px at 64x64 --
    // every triangle has real area, no consecutive-vertex collinearity.
    uint k = idx / 3u;
    uint corner = idx % 3u;
    float cx = -1.0 + float(k % 16u) * 0.125;
    float cy = -1.0 + float((k / 16u) % 11u) * (2.0 / 11.0);
    vec2 p = vec2(cx, cy);
    if(corner == 1u)
        p.x += 0.125;
    else if(corner == 2u)
        p.y += 2.0 / 11.0;

    geo_position_out[idx] = vec4(p, 0.0, 1.0);
    geo_color_out[idx] = vec4(1.0, float(k) / 176.0, 0.25, 1.0);
}
