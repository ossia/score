/*{
  "DESCRIPTION": "One white triangle drawn with POLYGON_MODE line. Used by GfxPolygonModeIE: line leaves the interior empty where the backend reports QRhi::NonFillPolygonMode, fill covers it.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "POLYGON_MODE": "line",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
