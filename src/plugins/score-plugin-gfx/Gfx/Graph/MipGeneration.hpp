#pragma once
#include <QtGui/private/qrhi_p.h>

#include <algorithm>

namespace score::gfx
{
//! Queue the mip chain of `tex`, unless its base level is 1x1: such a texture
//! has a single level, and Metal's generateMipmapsForTexture: rejects it with a
//! validation abort. Qt's D3D12 backend skips that case itself; its Metal one
//! does not.
inline void generateMipsIfAny(QRhiResourceUpdateBatch& res, QRhiTexture* tex)
{
  if(!tex)
    return;
  const QSize sz = tex->pixelSize();
  if(std::max(sz.width(), sz.height()) > 1)
    res.generateMips(tex);
}
}
