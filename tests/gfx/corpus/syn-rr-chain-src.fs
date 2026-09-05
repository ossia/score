/*{
  "DESCRIPTION": "Single-output RAW_RASTER_PIPELINE source stage for the raster->raster(MRT) chain test (GfxRasterChainMrt.cpp). Writes R = X ramp (0 at the left edge, 1 at the right) and the constant markers G = 0.25, B = 0.75, A = 1. The pattern is deliberately Y-INVARIANT: a downstream stage that samples this texture proves the chain is live through the G/B markers (an unconnected or black sampler reads 0) and through the per-pixel X ramp, without depending on the vertical orientation of the intermediate render target. Drive with a viewport-covering geometry producer (syn-geo-producer.cs).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-SYNTHETIC"],
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
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(v_uv.x, 0.25, 0.75, 1.0);
}
