/*{
  "DESCRIPTION": "Declares its own #define PI, as user shaders do. Stores sin(PI / 2) in red and PI / 4 in green. Any PI the engine declares in the text around it breaks the stage. Used by GfxDefinePiFrameLeakF3.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/
#define PI 3.1415926535

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;
    IMG_STORE(outputImage, pos, vec4(sin(PI * 0.5), PI / 4.0, 0.0, 1.0));
}
