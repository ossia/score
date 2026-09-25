/*{
  "DESCRIPTION": "MANUAL procedural raw raster whose invocation count is $BYTESIZE_params / 16, from the byte size of the buffer bound to its INPUTS uniform_input `params` through a Buffer edge. Every invocation redraws the target with red = (PASSINDEX + 1) / 255, so the red left on screen is the count.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-BINDING", "TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "counted" },
    { "TYPE": "vec4", "NAME": "spare" }
  ],
  "INPUTS": [
    { "NAME": "params", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "tint", "TYPE": "vec4" } ]
    }
  ],
  "EXECUTION_MODEL": { "TYPE": "MANUAL", "COUNT": "$BYTESIZE_params / 16" },
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
