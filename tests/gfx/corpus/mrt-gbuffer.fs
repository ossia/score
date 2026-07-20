/*{
  "DESCRIPTION": "MRT G-buffer generator. Takes an input image and produces 3 outputs: color (passthrough), normals (estimated from luminance gradients), and edge detection. Connect each output to a separate downstream node to inspect them independently.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MRT"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" },
    { "NAME": "normalStrength", "TYPE": "float", "DEFAULT": 2.0, "MIN": 0.1, "MAX": 10.0 },
    { "NAME": "edgeThreshold", "TYPE": "float", "DEFAULT": 0.1, "MIN": 0.01, "MAX": 0.5 }
  ],
  "OUTPUTS": [
    { "NAME": "colorOut" },
    { "NAME": "normalsOut" },
    { "NAME": "edgesOut" }
  ]
}*/

float luminance(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    vec2 uv = isf_FragNormCoord;
    vec2 texel = 1.0 / RENDERSIZE;

    vec4 center = IMG_NORM_PIXEL(inputImage, uv);

    // Sample neighbors for gradient estimation
    float lC = luminance(center.rgb);
    float lR = luminance(IMG_NORM_PIXEL(inputImage, uv + vec2(texel.x, 0.0)).rgb);
    float lL = luminance(IMG_NORM_PIXEL(inputImage, uv - vec2(texel.x, 0.0)).rgb);
    float lU = luminance(IMG_NORM_PIXEL(inputImage, uv + vec2(0.0, texel.y)).rgb);
    float lD = luminance(IMG_NORM_PIXEL(inputImage, uv - vec2(0.0, texel.y)).rgb);

    // Output 1: color passthrough
    colorOut = center;

    // Output 2: screen-space normals from luminance gradients
    float dx = (lR - lL) * normalStrength;
    float dy = (lU - lD) * normalStrength;
    vec3 normal = normalize(vec3(dx, dy, 1.0));
    normalsOut = vec4(normal * 0.5 + 0.5, 1.0);

    // Output 3: edge detection (Sobel-like)
    float edge = abs(lR - lL) + abs(lU - lD);
    float edgeMask = step(edgeThreshold, edge);
    edgesOut = vec4(vec3(edgeMask), 1.0);
}
