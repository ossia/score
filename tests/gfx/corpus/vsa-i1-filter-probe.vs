/*{
  "DESCRIPTION": "One triangle over the whole viewport, coloured with the red of the image input sampled at u = 0.4, v = 0.5. Used by GfxInputSamplerFilterI1 to see the inlet's filter change.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 3,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 1.0, 1.0],
  "INPUTS": [
    { "NAME": "src", "TYPE": "image" }
  ]
}*/

void main() {
  vec2 p = vec2(0.0);
  if(vertexId < 0.5)
    p = vec2(-3.0, -3.0);
  else if(vertexId < 1.5)
    p = vec2( 3.0, -3.0);
  else
    p = vec2( 0.0,  3.0);
  gl_Position = vec4(p, 0.0, 1.0);
  v_color = vec4(textureLod(src, vec2(0.4, 0.5), 0.0).r, 0.0, 0.0, 1.0);
}
