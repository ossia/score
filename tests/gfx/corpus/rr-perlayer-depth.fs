/*{
  "DESCRIPTION": "Procedural RAW_RASTER_PIPELINE with EXECUTION_MODEL=PER_LAYER over a 4-layer DEPTH Texture2DArray output (the shape shadow_cascades.frag has, without its scene dependencies). Each of the 4 invocations rasterises one fullscreen triangle at a per-layer closed-form clip z (rr-perlayer-depth.vs), and because Qt RHI 6.11 has no per-layer depth attachment the runtime must render into a shared scratch 2D D32F and copyTexture it into layer i after each endPass (RenderedRawRasterPipelineNode.hpp:260-279 declares the state, RenderedRawRasterPipelineNode.cpp:3199-3218 is the copy). Depths are 0.2/0.4/0.6/0.8, i.e. 51/102/153/204 when a downstream sampler2DArray probe writes them into an RGBA8 readback. Sibling of rr-perlayer.{vs,fs}, which drives the PER_LAYER *colour* path (m_perLayerIsDepth == false) and therefore never touches the copy shim.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-EXECUTION-MODEL", "TEST-DEPTH"],

  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [],

  "OUTPUTS": [
    { "NAME":   "cascade",
      "TYPE":   "depth",
      "FORMAT": "d32f",
      "LAYERS": 4,
      "WIDTH":  64,
      "HEIGHT": 64 }
  ],

  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "cascade" },

  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "always",
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  },

  "INPUTS": []
}*/

// DEPTH_COMPARE "always" is declared on purpose. Without it the raw-raster
// default is the project-wide reverse-Z GREATER paired with a clear of 0.0
// (depthClearForCompare in PipelineStateHelpers.cpp), which would silently
// reject any layer whose predicted depth were 0. "always" removes the compare
// from the oracle entirely: what lands in the buffer is the vertex stage's own
// z, nothing else.
//
// Fragment stage: empty. FRAGMENT_OUTPUTS is [] — depth write alone is the
// goal, same as shadow_cascades.frag. (No "COLOR_WRITE": false here: the ISF
// parser reads COLOR_WRITE as a *string* mask, so the boolean in
// shadow_cascades.frag is silently ignored.)
void main() { }
