/*{
  "DESCRIPTION": "MRT RAW_RASTER_PIPELINE consumer stage for the raster->raster(MRT) chain test (GfxRasterChainMrt.cpp): the topology of 2026/crash-renderpipeline-mrt.score. Two FRAGMENT_OUTPUTS plus one image INPUT sampled from the upstream single-output raw-raster stage (syn-rr-chain-src.fs). out0 = the upstream sample, verbatim: expected (X ramp, 0.25, 0.75, 1) -- the G/B markers prove the sampler is bound to a live upstream texture, not black/empty, and the sample is Y-invariant so no assumption is made about the intermediate texture's vertical orientation. out1 = this stage's OWN closed form (R = X ramp, G = Y ramp with 1 at the TOP row, B = 0.5, A = 1), which pins that the second attachment really carries the second output and in the pinned orientation. Drive with a viewport-covering geometry producer (syn-geo-producer.cs).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-MRT", "TEST-SYNTHETIC"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "out0" },
    { "TYPE": "vec4", "NAME": "out1" }
  ],
  "INPUTS": [
    { "NAME": "tex", "TYPE": "image" }
  ]
}*/

void main()
{
    out0 = IMG_NORM_PIXEL(tex, v_uv);
    out1 = vec4(v_uv.x, v_uv.y, 0.5, 1.0);
}
