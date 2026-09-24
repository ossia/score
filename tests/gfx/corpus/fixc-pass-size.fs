/*{
  "DESCRIPTION": "Pass 0 is sized from the input image ($WIDTH_inputImage / 2 x $HEIGHT_inputImage / 2); pass 1 outputs the ratio of that pass size to the input size in red and green (0.5 each when the per-input size variables resolve).",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ],
  "PASSES": [
    { "TARGET": "halfBuf", "WIDTH": "$WIDTH_inputImage / 2", "HEIGHT": "$HEIGHT_inputImage / 2" },
    {}
  ]
}*/
void main()
{
    if(PASSINDEX == 0)
    {
        gl_FragColor = vec4(1.0);
    }
    else
    {
        vec2 h = vec2(textureSize(halfBuf, 0));
        vec2 i = vec2(textureSize(inputImage, 0));
        gl_FragColor = vec4(h.x / i.x, h.y / i.y, 0.0, 1.0);
    }
}
