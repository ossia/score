/*{
  "DESCRIPTION": "Tests ISF image input with all sampling macros. Displays a 2x2 grid: top-left = IMG_NORM_PIXEL, top-right = IMG_THIS_PIXEL, bottom-left = IMG_PIXEL (absolute coords), bottom-right = TEX_DIMENSIONS readback. Pipeline: isf-solid-color.fs (or any ISF) -> this(inputImage) -> Window. If sampling macros work, all quadrants show the texture. If broken, quadrants will be black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    int col = int(floor(uv.x * 2.0));
    int row = int(floor(uv.y * 2.0));
    vec2 cellUV = fract(uv * 2.0);

    vec4 c = vec4(0.0);

    if(row == 0 && col == 0)
    {
        // IMG_NORM_PIXEL: normalized coords
        c = IMG_NORM_PIXEL(inputImage, cellUV);
    }
    else if(row == 0 && col == 1)
    {
        // IMG_THIS_PIXEL: uses isf_FragNormCoord directly
        c = IMG_THIS_PIXEL(inputImage);
    }
    else if(row == 1 && col == 0)
    {
        // IMG_PIXEL: absolute pixel coords
        vec2 absCoord = cellUV * RENDERSIZE;
        c = IMG_PIXEL(inputImage, absCoord);
    }
    else
    {
        // TEX_DIMENSIONS: show texture size encoded as color
        ivec2 sz = TEX_DIMENSIONS(inputImage);
        c = vec4(float(sz.x) / 2048.0, float(sz.y) / 2048.0, 0.5, 1.0);
    }

    // Grid border
    float border = step(0.01, cellUV.x) * step(0.01, cellUV.y)
                 * step(cellUV.x, 0.99) * step(cellUV.y, 0.99);

    gl_FragColor = c * border;
}
