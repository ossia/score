/*{
  "DESCRIPTION": "Red = 0.5 + 0.5 cos(2 pi x): continuous across x = 0 / 1, where it is 1, and 0 at x = 0.5. A known equirectangular content without a seam.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(0.5 + 0.5 * cos(6.28318530718 * isf_FragNormCoord.x), 0.2, 0.2, 1.0);
}
