/*{
  "DESCRIPTION": "Tests ISF multi-pass with custom WIDTH/HEIGHT expressions. Pass 0 renders into a small (64x64) buffer. Pass 1 reads that small buffer and upscales to output. If custom pass sizes work, you see a pixelated gradient (64x64 upscaled). If broken or sizes are ignored, you see a smooth gradient at full resolution.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MULTIPASS"],
  "INPUTS": [],
  "PASSES": [
    { "TARGET": "smallBuf", "WIDTH": 64, "HEIGHT": 64 },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        // Write a gradient into the small 64x64 buffer
        // Add a grid pattern so pixelation is visible
        float gridX = step(0.5, fract(uv.x * 8.0));
        float gridY = step(0.5, fract(uv.y * 8.0));
        float checker = abs(gridX - gridY);

        gl_FragColor = vec4(uv.x, uv.y, checker, 1.0);
    }
    else
    {
        // Read from the small buffer - should show visible pixelation
        gl_FragColor = IMG_NORM_PIXEL(smallBuf, uv);
    }
}
