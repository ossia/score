/*{
  "DESCRIPTION": "Emits one deliberately LOPSIDED triangle with three different vertex colours, as a geometry source whose rasterised picture is a function of what the compute shader wrote. syn-geo-producer.cs is the opposite by design: its triangle is oversized so it covers the viewport edge to edge in one flat colour, which makes it a perfect driver for tests about something else but a useless subject for a golden -- ANY position error that still covers the screen, and any colour error that is still constant, renders the identical frame. This one is the subject: the triangle occupies a closed-form 19.281 % of the frame, no vertex sits on either axis of symmetry, and the corners are red / green / blue, so a flip, a 180 rotation, a dropped or swapped attribute, a lost per-vertex colour and a wrong vertex position all change the picture. Fixed VERTEX_COUNT and no TIME anywhere, so the picture is the same on every run. Drive it into raw-raster-basic.fs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-GEOMETRY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
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

// Clip-space vertices. No projection matrix is involved in a raw-raster
// pipeline: what is written here IS gl_Position, so the [-1,1] square is the
// viewport whatever its aspect, and the triangle's share of the frame is its
// share of that square -- a number the test asserts in closed form.
//
//   v0 (-0.80, -0.60)  red
//   v1 ( 0.55, -0.20)  green
//   v2 (-0.10,  0.75)  blue
//
// Area = |(v1-v0) x (v2-v0)| / 2
//      = |(1.35, 0.40) x (0.70, 1.35)| / 2
//      = |1.35*1.35 - 0.40*0.70| / 2 = (1.8225 - 0.28) / 2 = 0.77125
// as a fraction of the square's area of 4: 0.77125 / 4 = 0.19281.
//
// The three vertices are distinct in both coordinates and none of the three
// edges is axis-parallel, so a horizontal flip, a vertical flip and a 180
// rotation each map the triangle onto a different set of pixels.

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
