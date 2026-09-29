/*{
  "DESCRIPTION": "ISF v1 form of isf-persistent-last-pass-accumulate.fs: persistence declared by the top-level PERSISTENT_BUFFERS list, the pass only names the buffer as its TARGET. After k rendered frames the output red is k*16.",
  "CREDIT": "test",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "PERSISTENT_BUFFERS": [ "acc" ],
  "PASSES": [ { "TARGET": "acc" } ]
}*/

void main()
{
    float prev = IMG_NORM_PIXEL(acc, isf_FragNormCoord).r;
    gl_FragColor = vec4(min(prev + 16.0 / 255.0, 1.0), 0.0, 0.0, 1.0);
}
