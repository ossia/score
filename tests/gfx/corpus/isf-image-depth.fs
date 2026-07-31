/*{
  "DESCRIPTION": "Tests ISF image input with DEPTH: true. Left half: color, right half: depth (via IMG_DEPTH_NORM_PIXEL). Pipeline: isf-mrt-depth-color.fs -> this(inputImage) -> Window. If depth sampling works, right half shows grayscale depth. If not, it's black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image", "DEPTH": true }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(uv.x < 0.5)
    {
        // Left: color
        vec2 cellUV = vec2(uv.x * 2.0, uv.y);
        gl_FragColor = IMG_NORM_PIXEL(inputImage, cellUV);
    }
    else
    {
        // Right: depth
        vec2 cellUV = vec2((uv.x - 0.5) * 2.0, uv.y);
        float d = IMG_DEPTH_NORM_PIXEL(inputImage, cellUV);
        gl_FragColor = vec4(d, d, d, 1.0);
    }
}
