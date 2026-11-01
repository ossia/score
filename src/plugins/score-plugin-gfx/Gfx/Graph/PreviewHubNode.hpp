#pragma once
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score_plugin_gfx_export.h>

#include <functional>
#include <memory>
#include <vector>

namespace score::gfx
{
/**
 * @brief The output that renders, for the editor's previews, the nodes no
 *        output of the patch renders.
 *
 * Each preview of such a node takes one of its inputs. The node renders into
 * a target of the size the preview asks, and the preview's tap reads it back
 * in this output's render list, as it would in a list of the patch. All of
 * them share this one render list and device: a chain several previews show
 * renders once.
 */
struct SCORE_PLUGIN_GFX_EXPORT PreviewHubNode final : OutputNode
{
  static constexpr int max_inputs = 32;

  PreviewHubNode();
  ~PreviewHubNode() override;

  //! A free input, or -1 when they are all taken.
  int acquireInput();
  void releaseInput(int k);
  bool anyInputUsed() const noexcept;
  //! The size the node on input `k` renders at.
  void setInputSize(int k, QSize size);
  QSize inputSize(int k) const noexcept;

  void startRendering() override { }
  void render() override;
  void onRendererChange() override { }
  bool canRender() const override { return true; }
  void stopRendering() override { }

  void setRenderer(std::shared_ptr<RenderList> r) override { m_renderer = r; }
  RenderList* renderer() const override { return m_renderer.lock().get(); }

  void createOutput(OutputConfiguration conf) override;
  void destroyOutput() override;
  void updateGraphicsAPI(GraphicsApi api) override;
  std::shared_ptr<RenderState> renderState() const override { return m_renderState; }
  TextureRenderTarget currentRenderTarget() const noexcept override;

  OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override { return m_conf; }

private:
  //! The render size of the list follows the largest input: the chains the
  //! inputs pull render at it unless a node asks for another size.
  QSize renderSize() const noexcept;
  //! After a size or an input changed: the render size follows the largest
  //! input, the targets of the inputs are rebuilt.
  void inputsChanged();
  void createTarget();
  void releaseTarget();

  Configuration m_conf;
  std::weak_ptr<RenderList> m_renderer;
  std::shared_ptr<RenderState> m_renderState;
  std::function<void()> m_onResize;
  std::vector<QSize> m_sizes;
  // The list's own target, which nothing draws into: it gives the render
  // state the descriptor the passes of the list are compatible with
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
};
}
