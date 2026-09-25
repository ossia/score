/*{
  "DESCRIPTION": "Stepped execution probe (N51). R and G carry the 'value' input with 16 bits of precision: value * 255 = R + G / 255 in bytes. B is FRAMEINDEX modulo 256.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": [
    { "NAME": "value", "TYPE": "float", "DEFAULT": 0.0, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    float x = clamp(value, 0.0, 1.0) * 255.0;
    float hi = floor(x);
    float lo = x - hi;
    float fi = mod(float(FRAMEINDEX), 256.0);
    gl_FragColor = vec4(hi / 255.0, lo, fi / 255.0, 1.0);
}
