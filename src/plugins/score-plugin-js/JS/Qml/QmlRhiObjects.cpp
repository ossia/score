#include "QmlRhiObjects.hpp"

#if __has_include(<QQuickRhiItem>)
#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::TextureInletItem)

namespace JS
{

TextureInletItem::TextureInletItem(QQuickItem* parent)
    : QQuickRhiItem{parent}
{
  setSize(QSizeF(1280, 720));
}

QQuickRhiItemRenderer* TextureInletItem::createRenderer()
{
  auto r = new TextureInletItemRenderer{};
  return r;
}

TextureInletItemRenderer::TextureInletItemRenderer()
    : QQuickRhiItemRenderer{}
{
}
void TextureInletItemRenderer::initialize(QRhiCommandBuffer* cb) { }

void TextureInletItemRenderer::synchronize(QQuickRhiItem* item)
{
  auto& self = *static_cast<TextureInletItem*>(item);
  self.renderer = this;
  self.texture = this->colorTexture();
}

void TextureInletItemRenderer::render(QRhiCommandBuffer* cb) { }

}
#endif
