/*{
  "DESCRIPTION": "Raw raster with one plain colour output declaring GENERATE_MIPS: left half red, right half green, so the top of its mip chain is (0.5, 0.5, 0). Used by GfxRawRasterGenerateMips (shader board R32).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "OUTPUTS": [ { "NAME": "color", "TYPE": "color", "GENERATE_MIPS": true } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "VERTEX_COUNT": 3, "TOPOLOGY": "triangles" }
}*/
void main()
{
    color = gl_FragCoord.x < 0.5 * RENDERSIZE.x ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);
}
