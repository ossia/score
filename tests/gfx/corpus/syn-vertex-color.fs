/*{
  "DESCRIPTION": "RAW_RASTER_PIPELINE consumer for syn-indirect-ladder.cs: draws the geometry with its per-vertex color, unmodified. Real strips arrive as (1, q/255, 0, 1) and read back as {255, q, 0, 255}; poison strips arrive as (0, 0, 1, 1) and read back as pure blue -- the test tells the fallback rungs apart by which of the two families a draw painted.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-INDIRECT"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color", "SEMANTIC": "color" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = v_color;
}
