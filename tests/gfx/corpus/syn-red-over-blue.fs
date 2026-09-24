/*{
  "DESCRIPTION": "Red over blue: the top half of the image is red, the bottom half blue, in ISF's own coordinates. Used by GfxModelDisplayTexture to tell whether a consumer reads it the right way up.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": []
}*/
void main()
{
    isf_FragColor = isf_FragNormCoord.y > 0.5 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
}
