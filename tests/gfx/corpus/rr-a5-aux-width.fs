/*{
  "DESCRIPTION": "Raw raster whose output is $COUNT_items texels wide, $COUNT_items being the element count of the upstream `items` auxiliary. Each texel writes red = its column / 255, so the rightmost texel reports the width minus one. Two colour outputs, so it takes the MRT path that sizes its own targets. Used by GfxAuxSizeA5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "column" },
    { "TYPE": "vec4", "NAME": "spare" }
  ],
  "OUTPUTS": [
    { "NAME": "column", "TYPE": "color", "FORMAT": "rgba8", "WIDTH": "$COUNT_items", "HEIGHT": 4 },
    { "NAME": "spare", "TYPE": "color", "FORMAT": "rgba8" }
  ],
  "AUXILIARY": [
    { "NAME": "items", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none"
  },
  "INPUTS": []
}*/

void main()
{
    column = vec4(floor(isf_FragCoord.x) / 255.0, 0.0, 0.0, 1.0);
    spare = vec4(0.0);
}
