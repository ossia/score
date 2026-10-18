/*{
  "DESCRIPTION": "VSA line loop: four corners of a square of half-size 0.8. As a loop it draws the square's four sides; as separate lines it draws two of them (bottom and top); as a strip, three (no left side).",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 4,
  "PRIMITIVE_MODE": "LINE_LOOP",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": []
}*/

void main() {
  // 0 bottom-left, 1 bottom-right, 2 top-right, 3 top-left.
  vec2 p = vec2(-0.8, -0.8);
  if(vertexId > 0.5 && vertexId < 2.5)
    p.x = 0.8;
  if(vertexId > 1.5)
    p.y = 0.8;
  gl_Position = vec4(p, 0.0, 1.0);
  v_color = vec4(1.0);
}
