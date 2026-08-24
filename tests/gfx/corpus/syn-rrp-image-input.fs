/*{
  "DESCRIPTION": "Minimal RAW_RASTER pipeline sampling an image input through IMG_NORM_PIXEL. Isolates whether an INPUTS image is emitted as a sampler in raw-raster fragment sources at all.",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec2", "NAME": "v_uv" } ],
  "INPUTS": [ { "NAME": "tex", "TYPE": "image" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ]
}*/
void main()
{
  isf_FragColor = IMG_NORM_PIXEL(tex, v_uv);
}
