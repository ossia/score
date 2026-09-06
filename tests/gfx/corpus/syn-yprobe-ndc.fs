/*{
  "DESCRIPTION": "Y-ORIENTATION PROBE, geometry side. Draws a procedural triangle covering exactly the +Y half of NDC and shades it solid white. No geometry input, no scene chain, no CSF -- the simplest possible path from NDC to the readback. The oracle is WHICH ROWS are lit: with the fixture's row-0-is-top convention, NDC +Y must land on the LOW row indices. A backend that lights the HIGH rows instead has an odd number of Y flips between the rasterizer and the readback. NOTE: VERTEX_COUNT belongs in PIPELINE_STATE (not top level) and CULL_MODE must be none, or the draw is empty / culled and the oracle goes vacuous.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-ORIENTATION"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  },
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
