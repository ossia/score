/*{
  "DESCRIPTION": "Draws whatever geometry it is given in flat white, shading by nothing at all. Scene tests need to see WHERE a mesh landed, and a shader that shades by colour or by a light term renders a correctly-placed but unlit mesh as black -- indistinguishable from a mesh that was never drawn. Reading only position removes that ambiguity, so a pixel oracle measures placement and only placement.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
