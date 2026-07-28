/*{
  "DESCRIPTION": "Tests ISF with two image inputs blended together. Pipeline: isf-solid-color.fs -> this(imageA), isf-time-uniforms.fs -> this(imageB) -> Window. Left half shows imageA, right half shows imageB, with soft blend in middle. If both textures bind: split-screen view. If only one binds: one half black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "imageA", "TYPE": "image" },
    { "NAME": "imageB", "TYPE": "image" },
    { "NAME": "splitPos", "TYPE": "float", "DEFAULT": 0.5, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    vec4 a = IMG_NORM_PIXEL(imageA, uv);
    vec4 b = IMG_NORM_PIXEL(imageB, uv);

    // Soft blend around split position
    float blend = smoothstep(splitPos - 0.02, splitPos + 0.02, uv.x);

    gl_FragColor = mix(a, b, blend);
}
