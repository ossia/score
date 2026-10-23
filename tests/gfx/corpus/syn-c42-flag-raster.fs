/*{
  "DESCRIPTION": "C42 raster consumer: reads the producer's bool 'flag' attribute as a float vertex input. A bool has no vertex format, so the pipeline must be refused with a warning instead of fetching it with whatever format the enum value casts to.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4",  "NAME": "position" },
    { "TYPE": "float", "NAME": "flag", "SEMANTIC": "flag" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "float", "NAME": "v_flag" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "float", "NAME": "v_flag" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/

void main()
{
  isf_FragColor = vec4(1.0, v_flag, 0.0, 1.0);
}
