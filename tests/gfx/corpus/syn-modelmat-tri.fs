/*{
  "DESCRIPTION": "Fullscreen-triangle raw-raster pass that encodes MODEL_MATRIX's translation column into the fragment colour, with NO MULTIVIEW. Control half of the MULTIVIEW binding-shift probe: with nothing writing the raster node's transform3d port, MODEL_MATRIX is identity, so translation is (0,0,0) and the +0.5 bias reads mid-grey. Pairs with syn-modelmat-tri-mv.fs, which is this shader plus MULTIVIEW:6 and a cubemap output -- the only difference between the two.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-MULTIVIEW"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec3", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec3", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(v_color, 1.0);
}
