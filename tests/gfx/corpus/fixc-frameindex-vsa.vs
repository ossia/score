/*{
  "DESCRIPTION": "VSA: one triangle covering the viewport centre, coloured FRAMEINDEX / 255 in red.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 3,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": []
}*/

void main() {
  vec2 p = vec2(0.0);
  if(vertexId < 0.5)
    p = vec2(-1.5, -1.5);
  else if(vertexId < 1.5)
    p = vec2( 1.5, -1.5);
  else
    p = vec2( 0.0,  1.5);
  gl_Position = vec4(p, 0.0, 1.0);
  v_color = vec4(float(FRAMEINDEX) / 255.0, 0.0, 0.0, 1.0);
}
