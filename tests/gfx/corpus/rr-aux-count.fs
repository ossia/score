/*{
  "DESCRIPTION": "MANUAL raw raster whose invocation count is $COUNT_items, the element count of the upstream geometry's `items` auxiliary. Every invocation redraws the whole target with red = (PASSINDEX + 1) / 255, so the red channel left on screen is the count. Two colour outputs, because a single-output shader takes the single-target path, which never runs the MANUAL loop. Wire: syn-aux-count.cs -> this -> Window.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-AUXILIARY", "TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "counted" },
    { "TYPE": "vec4", "NAME": "spare" }
  ],
  "AUXILIARY": [
    {
      "NAME": "items",
      "ACCESS": "read_only",
      "LAYOUT": [
        { "NAME": "data", "TYPE": "vec4[]" }
      ]
    }
  ],
  "EXECUTION_MODEL": { "TYPE": "MANUAL", "COUNT": "$COUNT_items" },
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none"
  },
  "INPUTS": []
}*/

void main()
{
    counted = vec4(float(PASSINDEX + 1) / 255.0, 0.0, 0.0, 1.0);
    spare = vec4(0.0);
}
