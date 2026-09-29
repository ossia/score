/*{
  "DESCRIPTION": "Paints the size of its storage image, which the renderer allocates at the render size, in pixels out of 255: R = width, G = height.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "scratch", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "VISIBILITY": "fragment" }
  ]
}*/

void main()
{
    gl_FragColor = vec4(vec2(imageSize(scratch)) / 255.0, 0.0, 1.0);
}
