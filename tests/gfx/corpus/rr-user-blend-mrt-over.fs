/*{
  "DESCRIPTION": "Procedural fullscreen raw raster with two colour outputs, each writing the straight colour (0.8, 0.4, 0.2, 0.5) under ALPHA straight / COMPOSITE over. Drawn with the node's Enable blend control on, the user's blend factors must see the same colour whatever COMPOSITE says.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-ALPHA", "TEST-RAW-RASTER"],
  "ALPHA": "straight",
  "COMPOSITE": "over",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "first" },
    { "TYPE": "vec4", "NAME": "second" }
  ],
  "INPUTS": [],
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
    first = vec4(0.8, 0.4, 0.2, 0.5);
    second = vec4(0.8, 0.4, 0.2, 0.5);
}
