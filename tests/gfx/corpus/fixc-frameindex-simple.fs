/*{
  "DESCRIPTION": "Single-pass ISF (SimpleRenderedISFNode): outputs FRAMEINDEX / 255 in red.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": []
}*/
void main()
{
    gl_FragColor = vec4(float(FRAMEINDEX) / 255.0, 0.0, 0.0, 1.0);
}
