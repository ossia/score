/*{
  "DESCRIPTION": "A single PERSISTENT pass adding 16/255 to its own previous red each frame. The node blits the persistent target to its output after the pass, so after k rendered frames the output red is k*16.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "PASSES": [ { "TARGET": "acc", "PERSISTENT": true } ]
}*/

void main()
{
    float prev = IMG_NORM_PIXEL(acc, isf_FragNormCoord).r;
    gl_FragColor = vec4(min(prev + 16.0 / 255.0, 1.0), 0.0, 0.0, 1.0);
}
