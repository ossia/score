/*{
  "DESCRIPTION": "Draws the incoming mesh with PRIMITIVE_DATA: red = PRIMITIVE_ID / 2, green and blue = the first two barycentric weights. Used by GfxRawRasterPrimitiveDataA5 with an indexed mesh whose triangles share a vertex.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "PRIMITIVE_DATA": true,
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none"
  },
  "INPUTS": []
}*/
void main()
{
    isf_FragColor = vec4(float(PRIMITIVE_ID) * 0.5, BARYCENTRIC.x, BARYCENTRIC.y, 1.0);
}
