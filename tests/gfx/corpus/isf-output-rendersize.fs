/*{
  "DESCRIPTION": "Paints the render size the render list's output uniforms hold, in pixels out of 255: R = width, G = height.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(isf_renderer_uniforms.RENDERSIZE_ / 255.0, 0.0, 1.0);
}
