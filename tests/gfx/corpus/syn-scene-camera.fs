/*{
  "DESCRIPTION": "Flat-white companion to syn-scene-camera.vs, which places geometry through the raw-raster camera built-in VIEWPROJECTION_MATRIX. Shading by nothing keeps a pixel oracle measuring PLACEMENT only: a shader shading by a light term would render a correctly-placed mesh black, indistinguishable from one never drawn.",
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
