#pragma once

#include <Gfx/Graph/Node.hpp>

#if QT_SVG_LIB
#include <QPainter>
#include <QSvgRenderer>
#endif

namespace score::gfx
{
enum ImageMode
{
  Single,
  Clamped,
  Tiled,
  Mirrored
};

/**
 * @brief A node that renders an image to screen.
 */
struct ImagesNode : NodeModel
{
public:
  explicit ImagesNode();
  virtual ~ImagesNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  class PreloadedRenderer;
  class OnTheFlyRenderer;

#pragma pack(push, 1)
  struct UBO
  {
    int currentImageIndex{};
    float opacity{1.};
    float position[2]{0.5, 0.5};
    float scale[2]{1., 1.};
  } ubo;
#pragma pack(pop)

  std::atomic_int imagesChanged{};
  std::atomic<ImageMode> tileMode{};
  score::gfx::ScaleMode scaleMode{score::gfx::ScaleMode::Original};
  float scale_w{1.0f};
  float scale_h{1.0f};

private:
  void clear();
  void process(Message&& msg) override;

  using image_type = std::variant<QImage*, QSvgRenderer*>;
  std::vector<score::gfx::Image> images;
  std::vector<image_type> linearImages;
};
struct FullScreenImageNode : NodeModel
{
public:
  explicit FullScreenImageNode(QImage dec);
  virtual ~FullScreenImageNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  class Renderer;

private:
  QImage m_image;
};
}
