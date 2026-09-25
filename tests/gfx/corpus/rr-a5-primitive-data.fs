/*{
  "DESCRIPTION": "Two triangles drawn from gl_VertexIndex: one pointing right from the left edge, one pointing left from the right edge, both symmetric about y = 0. PRIMITIVE_DATA gives red = PRIMITIVE_ID / 2, green and blue = the first two barycentric weights. Used by GfxRawRasterPrimitiveDataA5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "PRIMITIVE_DATA": true,
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 6,
    "TOPOLOGY": "triangles"
  },
  "INPUTS": []
}*/
void main()
{
    isf_FragColor = vec4(float(PRIMITIVE_ID) * 0.5, BARYCENTRIC.x, BARYCENTRIC.y, 1.0);
}
