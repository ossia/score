/*{
  "DESCRIPTION": "Reads the vertexshaderart touch history the ways VSA art does and paints one vertical band per read, left to right. Bands 0-6 show a touch sample as (x * 0.5 + 0.5, y * 0.5 + 0.5, pressure): 0 texture(v = 0), 1 texture(v = 0.01), 2 texture(row 5 centre), 3 texture(v = 1), 4 textureLod(row 5), 5 texelFetch(row 5), 6 column 1 at v = 0, 8 texture(v = 5 / 240), the edge between rows 4 and 5, which nearest filtering puts in row 5. Band 7: red if row 0's time is later than row 5's, green if row 0's time is this frame's, blue if the texture is 32x240. Used by GfxVsaTouch.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 54,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": [ { "LABEL": "Touch", "NAME": "touch", "TYPE": "image" } ]
}*/

vec4 show(vec4 t) { return vec4(t.xy * 0.5 + 0.5, t.z, 1.0); }

void main() {
  float band = floor(vertexId / 6.0);
  float k = mod(vertexId, 6.0);
  vec2 c = vec2(0.0);
  if(k < 0.5) c = vec2(0.0, 0.0);
  else if(k < 1.5) c = vec2(1.0, 0.0);
  else if(k < 2.5) c = vec2(0.0, 1.0);
  else if(k < 3.5) c = vec2(0.0, 1.0);
  else if(k < 4.5) c = vec2(1.0, 0.0);
  else c = vec2(1.0, 1.0);
  float x = -1.0 + 2.0 * (band + c.x) / 9.0;
  gl_Position = vec4(x, c.y * 2.0 - 1.0, 0.0, 1.0);

  vec4 col = vec4(0.0);
  if(band < 0.5)
    col = show(texture(touch, vec2(0., 0.)));
  else if(band < 1.5)
    col = show(texture(touch, vec2(0., 0.01)));
  else if(band < 2.5)
    col = show(texture(touch, vec2(0., 5.5 / 240.)));
  else if(band < 3.5)
    col = show(texture(touch, vec2(0., 1.)));
  else if(band < 4.5)
    col = show(textureLod(touch, vec2(0., 5.5 / 240.), 0.));
  else if(band < 5.5)
    col = show(texelFetch(touch, ivec2(0, 5), 0));
  else if(band < 6.5)
    col = show(texture(touch, vec2(1.5 / 32., 0.)));
  else if(band > 7.5)
    col = show(texture(touch, vec2(0., 5. / 240.)));
  else
  {
    float w0 = texture(touch, vec2(0., 0.)).w;
    float w5 = texture(touch, vec2(0., 5.5 / 240.)).w;
    col = vec4(
        w0 > w5 ? 1.0 : 0.0,
        w0 == time ? 1.0 : 0.0,
        textureSize(touch, 0) == ivec2(32, 240) ? 1.0 : 0.0,
        1.0);
  }
  v_color = col;
}
