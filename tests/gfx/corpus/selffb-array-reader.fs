/*{
  "DESCRIPTION": "Shows layer 0 of an array input.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "arr", "TYPE": "image", "ARRAY": true }
  ]
}*/
void main()
{
    gl_FragColor = texture(arr, vec3(isf_FragNormCoord, 0.0));
}
