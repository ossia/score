/*{
  "DESCRIPTION": "The soft-edge blend ramp of the multi-window output, evaluated over a coordinate that steps outside [0;1] the way the fragment centre of a partially covered edge pixel does. Green carries the alpha the ramp produces; red 0.25 / blue 0.75 are garbage detectors. Without a clamp, pow() is called on a negative base -- undefined in GLSL -- and the drivers that answer NaN turn the leftmost pixels fully lit, which is the one-pixel bright fringe this pins.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BLEND"],
  "INPUTS": []
}*/

void main()
{
    // Spans [-0.125; 1.125]: the first eighth of the image is "outside" the
    // quad, exactly as an extrapolated edge coordinate is.
    float t = isf_FragNormCoord.x * 1.25 - 0.125;

    const float width = 0.25;
    const float gamma = 2.0;

    float alpha = 1.0;
    if(width > 0.0 && t < width)
        alpha *= pow(clamp(t / width, 0.0, 1.0), gamma);
    alpha = clamp(alpha, 0.0, 1.0);

    gl_FragColor = vec4(0.25, alpha, 0.75, 1.0);
}
