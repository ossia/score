/*{
  "DESCRIPTION": "Raw raster reading a depth Texture2DArray through an IS_ARRAY image input. Paints red = depth of layer 1 at the centre, green = layer count / 8, blue = width / 256, so a test can tell the upstream array from the 1x1 fallback a GL driver substitutes for an incomplete texture.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-DEPTH"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "layers", "TYPE": "image", "IS_ARRAY": true }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    ivec3 sz = textureSize(layers, 0);
    isf_FragColor = vec4(
        texture(layers, vec3(0.5, 0.5, 1.0)).r,
        float(sz.z) / 8.0,
        float(sz.x) / 256.0,
        1.0);
}
