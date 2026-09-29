/*{
  "DESCRIPTION": "A flat 2D ISF that writes gl_FragDepth, which is enough for the node to ask its render list for depth. Paints the right half solid blue at depth 0.5 and discards the left half, so whatever shares its target stays visible there.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": []
}*/

void main()
{
    if(isf_FragNormCoord.x < 0.5)
        discard;
    gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
    gl_FragDepth = 0.5;
}
