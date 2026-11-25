/*{
  "DESCRIPTION": "One-pixel black and white checkerboard: the worst case for a minifying resample (mean 0.5).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": []
}*/

void main()
{
    float c = mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0);
    gl_FragColor = vec4(vec3(c), 1.0);
}
