/*{
  "DESCRIPTION": "GfxRawRasterFixK (N70): a single colour OUTPUT declared rgba16f. 256 additive fullscreen layers of 1/1024 each should sum to 0.25 (64/255) in a float target; in an 8-bit target every layer rounds away and the frame stays black. Alpha is written 1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "accum", "TYPE": "color", "FORMAT": "rgba16f" } ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "TOPOLOGY": "triangles",
    "VERTEX_COUNT": 3, "INSTANCE_COUNT": 256,
    "BLEND": { "ENABLE": true, "SRC_COLOR": "one", "DST_COLOR": "one", "OP_COLOR": "add", "SRC_ALPHA": "one", "DST_ALPHA": "zero", "OP_ALPHA": "add" }
  }
}*/

void main()
{
    isf_FragColor = vec4(vec3(1.0 / 1024.0), 1.0);
}
