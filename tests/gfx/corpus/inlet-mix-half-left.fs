/*{
  "DESCRIPTION": "Paints the left half of the target and discards the rest, so two of these cabled into one texture inlet show whether the inlet mixes its cables.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": []
}*/

void main()
{
    if(!(isf_FragNormCoord.x < 0.5))
        discard;
    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
