/*{
  "DESCRIPTION": "RAW_RASTER_PIPELINE with four FRAGMENT_OUTPUTS, each carrying a pattern that is asymmetric on BOTH axes and unique per attachment: R encodes X (0 at the left edge, 1 at the right), G encodes Y (1 at the TOP row, 0 at the bottom), B identifies the attachment (k / 3), A is 1. The raw-raster twin of isf-mrt-pattern.fs: a vertical flip, a horizontal flip, a 180 rotation, an attachment permutation and an R/B swizzle each produce a different picture, so none of them can pass. Drive with a viewport-covering geometry producer (syn-geo-producer.cs).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-MRT", "TEST-ORIENTATION"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "out0" },
    { "TYPE": "vec4", "NAME": "out1" },
    { "TYPE": "vec4", "NAME": "out2" },
    { "TYPE": "vec4", "NAME": "out3" }
  ],
  "INPUTS": []
}*/

void main()
{
    float x = v_uv.x;
    float y = v_uv.y;
    out0 = vec4(x, y, 0.0 / 3.0, 1.0);
    out1 = vec4(x, y, 1.0 / 3.0, 1.0);
    out2 = vec4(x, y, 2.0 / 3.0, 1.0);
    out3 = vec4(x, y, 3.0 / 3.0, 1.0);
}
