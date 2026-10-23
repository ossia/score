/*{
  "DESCRIPTION": "GPU-only particle chain, texture feedback. Pass 0 adds 1/16 of the input to a PERSISTENT target each frame, so a pixel lit for k frames reads min(k/16, 1). Pass 1 shows it. Without persistence the output stays at 1/16 of the input.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ],
  "PASSES": [
    { "TARGET": "acc", "PERSISTENT": true, "FLOAT": true, "FILTER": "NEAREST" },
    {}
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    vec4 prev = IMG_NORM_PIXEL(acc, isf_FragNormCoord);
    vec4 cur = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord);
    gl_FragColor = vec4(min(prev.rgb + cur.rgb / 16.0, vec3(1.0)), 1.0);
  }
  else
  {
    gl_FragColor = IMG_NORM_PIXEL(acc, isf_FragNormCoord);
  }
}
