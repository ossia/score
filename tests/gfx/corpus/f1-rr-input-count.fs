/*{
  "DESCRIPTION": "MANUAL procedural raw raster whose invocation count is $COUNT_items, the element count of the buffer bound to its INPUTS storage_input `items` through a Buffer edge. Every invocation redraws the target with red = (PASSINDEX + 1) / 255, so the red left on screen is the count. Wire: f1-storage-items.cs -> this -> Window.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-STORAGE", "TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "counted" },
    { "TYPE": "vec4", "NAME": "spare" }
  ],
  "INPUTS": [
    { "NAME": "items", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ]
    }
  ],
  "EXECUTION_MODEL": { "TYPE": "MANUAL", "COUNT": "$COUNT_items" },
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
    counted = vec4(float(PASSINDEX + 1) / 255.0, 0.0, 0.0, 1.0);
    spare = vec4(0.0);
}
