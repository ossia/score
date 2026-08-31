/*{
  "DESCRIPTION": "Four declared OUTPUTS, each carrying a pattern that is asymmetric on BOTH axes and unique per attachment: R encodes X, G encodes Y, B identifies the attachment. A vertical flip, a horizontal flip, a 180 rotation, an attachment permutation and an R/B swizzle each produce a different picture, so none of them can pass.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ORIENTATION", "TEST-MRT"],
  "INPUTS": [],
  "OUTPUTS": [
    { "NAME": "out0" },
    { "NAME": "out1" },
    { "NAME": "out2" },
    { "NAME": "out3" }
  ]
}*/

void main()
{
    float x = isf_FragNormCoord.x;
    float y = isf_FragNormCoord.y;
    out0 = vec4(x, y, 0.0 / 3.0, 1.0);
    out1 = vec4(x, y, 1.0 / 3.0, 1.0);
    out2 = vec4(x, y, 2.0 / 3.0, 1.0);
    out3 = vec4(x, y, 3.0 / 3.0, 1.0);
}
