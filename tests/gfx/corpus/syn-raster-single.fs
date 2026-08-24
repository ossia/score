/*{
  "DESCRIPTION": "EXECUTION_MODEL SINGLE, the explicit form of the default: exactly one pass, so PASSINDEX must be 0. Painting PASSINDEX into red means a runtime that looped the pass would leave the last index behind instead, and one that never ran leaves green at 0.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-EXECUTION-MODEL"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "EXECUTION_MODEL": { "TYPE": "SINGLE" },
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(float(PASSINDEX) / 255.0, 1.0, 0.0, 1.0);
}
