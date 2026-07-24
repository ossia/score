/*{
  "DESCRIPTION": "Two image inputs where the FIRST is a SamplableDepth port (DEPTH:true). imgA's depth is referenced so its depth-companion sampler exists (initInputSamplers pushes TWO QRhiSamplers for a SamplableDepth port). imgB is sampled at x in [1,2] — OUTSIDE [0,1] — so its result depends entirely on imgB's sampler ADDRESS mode: REPEAT wraps back to a 0->1 ramp, CLAMP_TO_EDGE pins to the right edge (red ~ 1.0). A runtime render_target_spec that switches imgB to CLAMP must edit imgB's OWN sampler; the sampler-index miscount across the SamplableDepth port edited imgA's depth companion instead, leaving imgB stuck on REPEAT.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE", "TEST-BINDING"],
  "INPUTS": [
    { "NAME": "imgA", "TYPE": "image", "DEPTH": true },
    { "NAME": "imgB", "TYPE": "image" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    // Reference imgA's depth so the SamplableDepth companion sampler is created
    // (this is the port whose presence shifts imgB's sampler index).
    float d = IMG_DEPTH_NORM_PIXEL(imgA, uv);

    // Sample imgB one full period to the right: coord.x in [1, 2). With REPEAT
    // this wraps to a 0->1 ramp; with CLAMP_TO_EDGE it stays at the right edge.
    vec4 b = IMG_NORM_PIXEL(imgB, vec2(uv.x + 1.0, uv.y));

    gl_FragColor = vec4(b.r + d * 0.0001, b.g, b.b, 1.0);
}
