#pragma once

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/IsfBindingsBuilder.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/detail/small_flat_map.hpp>

#include <span>

namespace score::gfx
{
// Used for the simple case of a single, non-persistent pass (the most common case)

struct RenderedRawRasterPipelineNode : score::gfx::NodeRenderer
{
  explicit RenderedRawRasterPipelineNode(const ISFNode& node) noexcept;

  virtual ~RenderedRawRasterPipelineNode();

  void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr) override;
  QRhiTexture* textureForOutput(const Port& output) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  bool updateMaterials(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge);
  void release(RenderList& r) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& renderer) override;
  void addOutputPass(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;
  void addInputEdge(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeInputEdge(RenderList& renderer, Edge& edge) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

  void process(int32_t port, const ossia::transform3d& v) override;

private:
  // Resolves every image-style INPUT against the incoming geometry's
  // auxiliary_textures list and overrides the initial texture pointer in
  // m_inputSamplers for matches. Also builds m_auxTextureBindings so
  // update() can cheaply re-run the lookup when the geometry changes.
  // Must be called AFTER initInputSamplers.
  void bindAuxTexturesInit(RenderList& renderer);

  // Per-frame update hook: walks m_auxTextureBindings, re-resolves each
  // binding's texture pointer from the current geometry's aux textures,
  // and returns true if at least one sampler's texture pointer changed
  // (caller will flag mustRecreatePasses).
  bool rebindAuxTextures();

  void initPass(
      const TextureRenderTarget& rt, RenderList& renderer,
      QRhiResourceUpdateBatch& res, Edge& edge);
  void initMRTPass(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void initMRTBlitPasses(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void initMRTBlitPass(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge& edge);

  // EXECUTION_MODEL=MANUAL: evaluate the COUNT expression against the
  // live input state (first input image's $WIDTH / $HEIGHT / $DEPTH /
  // $LAYERS, scalar input values as $<inputName>). Pure-integer literal
  // fast path; otherwise delegate to ossia::math_expression with '$' →
  // 'var_' rewrite — same convention as CSF STRIDE / image-size
  // expressions. Returns >= 1; unparseable expressions degrade to 1.
  int resolveManualInvocationCount() const;

  // True when the shader renders procedurally: no VERTEX_INPUTS, driven by
  // gl_VertexIndex, with PIPELINE_STATE.VERTEX_COUNT set. m_mesh stays null and
  // the draw skips vertex-buffer bindings, which relaxes the "no mesh, bail out"
  // guards for fullscreen passes, VSA-style draws and IBL precompute shaders.
  bool isProceduralDraw() const noexcept;

  // Evaluate an integer-valued expression against the same variable
  // surface as resolveManualInvocationCount ($WIDTH_<inp> / $HEIGHT /
  // scalar inputs). Used for OUTPUTS.WIDTH / HEIGHT at init time.
  // Returns `fallback` when the expression is empty, >=1 otherwise.
  int resolveIntExpression(const std::string& expr, int fallback) const;

  // Issue the draw for the currently bound pipeline and SRB. When the input
  // geometry carries several sub-meshes with per-mesh aux buffers, this iterates
  // them and re-points the SRB bindings before each draw; single-sub-mesh and MDI
  // geometries delegate to the mesh's own draw(). The SRB is left on the last
  // sub-mesh's bindings, and the next runRenderPass rebinds from scratch.
  void drawWithPerMeshAuxRebind(
      QRhiShaderResourceBindings& srb, QRhiCommandBuffer& cb,
      std::span<const FallbackBindingPlan::Slot> fallback_slots = {});

  std::vector<Sampler> allSamplers() const noexcept;

  ossia::small_vector<std::pair<Edge*, Pass>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;
  ossia::small_flat_map<Edge*, QRhiSampler*, 4> m_blitSamplersByEdge;

  int64_t meshChangedIndex{-1};
  const Mesh* m_mesh{};
  MeshBuffers m_meshbufs;

  // Quad mesh used for MRT blit passes (separate from the geometry mesh)
  const Mesh* m_blitMesh{};
  MeshBuffers m_blitMeshbufs;

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  QRhiBuffer* m_modelUBO{};

  struct AuxiliarySSBO
  {
    QRhiBuffer* buffer{};
    QRhiBuffer* prev_buffer{}; //!< Only set when persistent == true: the other half of the ping-pong pair.
    int64_t size{};
    bool owned{true}; // false when adopted from upstream geometry / upstream port
    bool is_uniform{false}; // true for uniform_input, false for storage_input
    bool persistent{false}; //!< Ping-pong pair swapped each frame (raw raster AUXILIARY only)
    std::string name;
    std::string access;
    // Index into n.input[] for the score port that may carry an upstream-
    // supplied QRhiBuffer*. -1 when the buffer can only come from the
    // input geometry's auxiliary list (e.g. desc.auxiliary entries without
    // a matching INPUTS port).
    int input_port_index{-1};
    // SRB binding slot assigned at pipeline build time. Needed so the per-
    // sub-mesh draw loop can patch `per_draw` (and any other per-mesh aux)
    // to point at mesh[i]'s buffer before drawing sub-mesh i. -1 when the
    // aux was filtered out of the SRB (e.g. visibility==none).
    int binding{-1};
    // For persistent aux only: binding slot of the <name>_prev (read-only)
    // half of the ping-pong pair. prev_binding + 1 == binding.
    int prev_binding{-1};
    //! Byte size of the block the shader declares (std140 for is_uniform,
    //! std430 otherwise), computed from the descriptor's LAYOUT alone.
    //! Independent of `size`, which is only ever set where a buffer already
    //! exists — so this is what the disconnected-placeholder path must use.
    int64_t declared_size{};
  };
  std::vector<AuxiliarySSBO> m_auxiliarySSBOs;

  // Storage images and the rest of the INPUTS storage trio -- storage_input for
  // SSBOs, csf_image_input for image2D/3D, uniform_input for UBOs -- declared in
  // the top-level INPUTS array. Wired through the shared IsfBindingsBuilder
  // helpers so the SRB binding type matches isf_emit_graphics_storage's GLSL.
  // m_auxiliarySSBOs carries only the AUXILIARY-block entries; the dual
  // population is deliberate while the AUXILIARY path still has its own dispatch.
  GraphicsStorageResources m_storage;
  int m_firstStorageBinding{-1};

  // Texture auxes carried on the input geometry (see
  // ossia::geometry::auxiliary_textures). Each entry records a sampler
  // slot in m_inputSamplers that auto-resolves its texture pointer from
  // the incoming geometry's aux-texture list by name at init() time and
  // again every time the geometry changes. Eliminates the need for a
  // dedicated texture cable (base_color_array / skybox / ...).
  struct AuxTextureBinding
  {
    int sampler_idx{-1}; // index into m_inputSamplers
    std::string name;    // INPUT name, matched against auxiliary_texture::name
  };
  std::vector<AuxTextureBinding> m_auxTextureBindings;

  // Non-owning per-port sampler overrides published by the upstream geometry's
  // auxiliary_texture::sampler_handle, parallel to m_inputSamplers. Kept separate
  // because m_inputSamplers entries are owned and released with
  // `delete sampler.sampler`, which would double-free a registry-owned sampler.
  std::vector<QRhiSampler*> m_inputSamplerOverrides;

  // Textures declared in the top-level AUXILIARY array (TYPE: image /
  // texture / cubemap / image_cube). Do NOT create a score input port —
  // resolved only from ossia::geometry::auxiliary_textures by name, with
  // a placeholder bound until the first matching handle arrives.
  struct AuxTextureAuxSampler
  {
    QRhiSampler* sampler{};  // Null for storage-image entries.
    QRhiTexture* texture{};
    // Shape-matched empty fallback (one of the RenderList-owned empty
    // textures). Set at init from is_cubemap / dimensions / is_array and
    // never changes. When rebindAuxTextures stops finding a matching
    // aux_texture upstream (producer stopped publishing the name, got
    // disconnected, etc.) we revert `texture` to this placeholder rather
    // than leaving the previous (possibly-freed) upstream handle in
    // place. Never owned by us.
    QRhiTexture* placeholder{};
    std::string name;
    int binding{-1};
    // Storage-image variant: bound with imageLoad / imageStore /
    // imageLoadStore instead of sampledTexture. `access` distinguishes
    // which of the three — "read_only" / "write_only" / "read_write".
    bool is_storage{false};
    std::string access;
  };
  std::vector<AuxTextureAuxSampler> m_auxTextureSamplers;

  std::optional<AudioTextureUpload> m_audioTex;

  // MRT: internally-owned render target with multiple attachments
  TextureRenderTarget m_mrtRenderTarget;
  bool m_hasMRT{false};
  bool m_mrtRenderedThisFrame{false};

  // EXECUTION_MODEL (top-level, RAW_RASTER only).
  //   Single   default, one invocation, no extra loop.
  //   PerMip   one invocation per mip level of the TARGET output, each binding a
  //            per-mip render target so the draw writes only that mip;
  //            ProcessUBO.passIndex carries the mip index. Drives the
  //            prefiltered-GGX roughness sweep.
  //   PerLayer one invocation per array layer. Colour targets bind setLayer(i);
  //            depth targets render to a shared scratch and copyTexture into
  //            layer i after the pass, since Qt RHI 6.11 has no per-layer depth
  //            attachment. Drives shadow_cascades.frag.
  //   Manual   count decided every frame by evaluating a COUNT expression through
  //            the math_expression parser, with the same variable surface as CSF
  //            STRIDE and image-size expressions. All invocations share the MRT
  //            render target and the shader branches on ProcessUBO.passIndex.
  enum class ExecutionMode : std::uint8_t
  {
    Single,
    PerMip,
    PerCubeFace,   // Iterate 6 cube faces; target = CubeMap + setLayer(i)
    PerLayer,      // Iterate N array layers; target = TextureArray + setLayer(i)
    Manual
  };
  ExecutionMode m_executionMode{ExecutionMode::Single};

  // PerCubeFace state. The target OUTPUT is allocated with
  // QRhiTexture::CubeMap (6 implicit layers) and six per-face render
  // targets are built at init; runInitialPasses iterates them in order,
  // stamping the face index into ProcessUBO.passIndex. Shares the
  // m_perMipOutputIndex resolution path (same "which colour output is
  // the target" question) and reuses the m_mipRTs vector for storage
  // — interpretation is mode-dependent (mip level vs face index).
  int m_perCubeFaceOutputIndex{-1};

  // PerMip state. When PerMip is active the MRT target texture is
  // allocated with QRhiTexture::MipMapped and m_mipCount / m_mipRTs
  // point at per-level render-pass views of it. m_perMipOutputIndex is
  // the index into m_mrtRenderTarget{.texture, .additionalColorTextures}
  // that we iterate. -1 in other modes.
  int m_perMipOutputIndex{-1};
  int m_mipCount{0};
  struct MipRT
  {
    QRhiTextureRenderTarget* renderTarget{};
    QRhiRenderPassDescriptor* renderPass{};
    QRhiTexture* depth{}; // per-level depth — owned here.
  };
  std::vector<MipRT> m_mipRTs;

  // PerLayer state. m_perLayerOutputIndex is the raw index into
  // descriptor().outputs[], depth-inclusive, unlike the colour-only
  // m_perMipOutputIndex / m_perCubeFaceOutputIndex. m_perLayerIsDepth picks the
  // path:
  //
  //   colour: m_mipRTs holds one entry per layer with a setLayer(i) attachment,
  //   structurally PER_CUBE_FACE with a variable layer count.
  //
  //   depth: Qt RHI 6.11 exposes no per-layer depth attachment, so
  //   m_perLayerScratchDepth is a single 2D D32F shared across iterations
  //   (m_perLayerSharedRT/RP) and runInitialPasses copies it into layer i after
  //   each endPass. m_perLayerOutputDepthArray aliases the OUTPUT array as the
  //   copy destination.
  int  m_perLayerOutputIndex{-1};
  bool m_perLayerIsDepth{false};
  QRhiTexture*              m_perLayerScratchDepth{nullptr};
  QRhiTexture*              m_perLayerDummyColor{nullptr};
  QRhiTextureRenderTarget*  m_perLayerSharedRT{nullptr};
  QRhiRenderPassDescriptor* m_perLayerSharedRP{nullptr};
  QRhiTexture*              m_perLayerOutputDepthArray{nullptr};

  // Manual state. Re-evaluated every frame in runInitialPasses.
  int m_manualCount{1};

  // Per-invocation UBO + SRB pool for PER_MIP / PER_CUBE_FACE / MANUAL.
  //
  // A QRhi Dynamic UBO has a single slot per frame-in-flight: several
  // updateDynamicBuffer calls to one buffer within a frame overwrite each other,
  // so every draw that frame reads the last value and distinct PASSINDEX values
  // collapse to one.
  //
  // One UBO and one SRB per invocation, pre-built at init, lets the render loop
  // swap which SRB it binds. Index 0 is the main pass (pass.processUBO /
  // pass.p.srb); the vectors hold 1..N-1 and grow lazily, which MANUAL needs
  // since its count is per-frame dynamic.
  std::vector<QRhiBuffer*> m_perInvocationUBOs;
  std::vector<QRhiShaderResourceBindings*> m_perInvocationSRBs;

  // Transparent CUBEMAP + MULTIVIEW shim: QRhi forbids setMultiViewCount on a
  // cube texture, so a shader declaring both renders into a hidden 2D
  // TextureArray -- the only shape multiview accepts -- and each layer is blitted
  // onto the matching cube face at the end of runInitialPasses. Downstream sees a
  // real samplerCube through textureForOutput(); the shadow array never leaves
  // this class.
  //
  //   m_cubeCopyShadowArray  6-layer TextureArray, UsedAsTransferSource, the
  //                          multiview render target
  //   m_cubeCopyCube         the public CubeMap
  //   m_cubeCopyOutputIdx    colour-attachment index taking the array-then-copy
  //                          path, -1 otherwise; one output per shader
  QRhiTexture* m_cubeCopyShadowArray{};
  QRhiTexture* m_cubeCopyCube{};
  int m_cubeCopyOutputIdx{-1};

  // The part of the m_materialUBO for which changes
  // trigger a pipeline recreation (blend status etc.)
  static constexpr int size_of_pipeline_material = 32;
  alignas(4) char m_prevPipelineChangingMaterial[size_of_pipeline_material]{0};
  struct PipelineChangingMaterial
  {
    int32_t mode;         // tri, point, line
    int32_t enable_blend; // bool
    QRhiGraphicsPipeline::BlendFactor src_color;
    QRhiGraphicsPipeline::BlendFactor dst_color;
    QRhiGraphicsPipeline::BlendOp op_color;
    QRhiGraphicsPipeline::BlendFactor src_alpha;
    QRhiGraphicsPipeline::BlendFactor dst_alpha;
    QRhiGraphicsPipeline::BlendOp op_alpha;
  };
  static_assert(sizeof(PipelineChangingMaterial) == size_of_pipeline_material);

  ossia::transform3d m_modelTransform;
};
}
