/*{
  "DESCRIPTION": "Uses 'quad' as a function name and 'operator' as an input name. Both are legal GLSL and reserved in the Metal Shading Language, where SPIRV-Cross does not rename them. Used by GfxMslReservedNames: renders red on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [ { "NAME": "operator", "TYPE": "float", "DEFAULT": 1.0 } ]
}*/
vec4 quad(float v)
{
    return vec4(v, 0.0, 0.0, 1.0);
}

void main()
{
    isf_FragColor = quad(operator);
}
