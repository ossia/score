/*{
  "DESCRIPTION": "Declares its own #define PI, as user shaders do. Paints sin(PI / 2) in red and PI / 4 in green. Any PI the engine declares in the text around it breaks the stage. Used by GfxDefinePiFrameLeakF3.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": []
}*/
#define PI 3.1415926535

void main()
{
    isf_FragColor = vec4(sin(PI * 0.5), PI / 4.0, 0.0, 1.0);
}
