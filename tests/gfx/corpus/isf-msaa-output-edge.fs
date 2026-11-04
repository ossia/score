/*{
  "DESCRIPTION": "A white square, half the frame and turned so that none of its edges is aligned with the pixel grid, drawn by its vertex shader straight into whatever this node's output is cabled to. Over black, a single-sample target only ever holds black or white; a multisampled one resolves the pixels the edges cross to greys. Used by GfxOffscreenOutputMsaa.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": []
}*/
void main()
{
  gl_FragColor = vec4(1.0);
}
