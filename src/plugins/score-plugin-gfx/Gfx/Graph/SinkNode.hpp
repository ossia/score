#pragma once
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <chrono>
#include <memory>

namespace score::gfx
{
/**
 * @brief An output that draws nowhere.
 *
 * The render graph only runs the nodes that lead to an output, so a node
 * whose result is data (an AI model's Data outlet, an analysis) and whose
 * texture goes nowhere never ran. This output pulls its input into an
 * offscreen target at `rate` frames per second, and shows nothing.
 *
 * The render clock ticks at the application's render rate; frames between
 * two due ones are skipped, so the rate can change while playing without
 * rebuilding the clocks. It is capped by the application's rate.
 */
class SCORE_PLUGIN_GFX_EXPORT SinkNode final : public OutputNode
{
public:
  explicit SinkNode(double rate);
  ~SinkNode() override;

  void setRate(double fps) noexcept;
  double rate() const noexcept { return m_rate; }

  void process(Message&& msg) override;

  void startRendering() override;
  void render() override;
  bool canRender() const override;
  void onRendererChange() override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<RenderList> r) override;
  RenderList* renderer() const override;

  void createOutput(OutputConfiguration conf) override;
  void destroyOutput() override;
  void updateGraphicsAPI(GraphicsApi api) override;

  std::shared_ptr<RenderState> renderState() const override;
  TextureRenderTarget currentRenderTarget() const noexcept override;
  OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  // Frames rendered so far, for tests.
  int64_t frames() const noexcept { return m_frames; }

private:
  std::weak_ptr<RenderList> m_renderer;
  std::shared_ptr<RenderState> m_renderState;
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};

  double m_rate{30.};
  std::chrono::steady_clock::time_point m_last{};
  int64_t m_frames{};
};
}
