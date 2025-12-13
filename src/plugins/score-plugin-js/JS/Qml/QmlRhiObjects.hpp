#pragma once

#if __has_include(<QQuickRhiItem>)
#include <QQuickRhiItem>

#include <verdigris>

namespace JS
{
struct TextureInletItem;
struct TextureInletItemRenderer : public QQuickRhiItemRenderer
{
public:
  explicit TextureInletItemRenderer();
  void initialize(QRhiCommandBuffer* cb) override;
  void synchronize(QQuickRhiItem* item) override;
  void render(QRhiCommandBuffer* cb) override;
};

struct TextureInletItem : public QQuickRhiItem
{
  W_OBJECT(TextureInletItem)
public:
  explicit TextureInletItem(QQuickItem* parent = nullptr);

  TextureInletItemRenderer* renderer{};
  QRhiTexture* texture{};
  QQuickRhiItemRenderer* createRenderer() override;
};

}
#endif
