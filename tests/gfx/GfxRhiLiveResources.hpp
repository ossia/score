#pragma once
// QRhi's own registry of the resources it created and has not destroyed yet,
// and the raw data of a shader resource binding: private QRhi state, whose
// shape changed in Qt 6.5 (QSet -> QHash) and 6.6 (qrhi_p_p.h folded into
// qrhi_p.h, Data only reachable through QRhiImplementation).
#include <QtGui/private/qrhi_p.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
#include <QtGui/private/qrhi_p_p.h>
#endif

namespace score::test::gfx
{
namespace
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
using LiveResources = QHash<QRhiResource*, bool>;

LiveResources& live_resources(QRhiImplementation& rhi);

template <LiveResources QRhiImplementation::* Member>
struct live_resources_access
{
  friend LiveResources& live_resources(QRhiImplementation& rhi) { return rhi.*Member; }
};
template struct live_resources_access<&QRhiImplementation::resources>;

inline QRhiResource* live_resource(LiveResources::const_iterator it)
{
  return it.key();
}
#else
using LiveResources = QSet<QRhiResource*>;

inline LiveResources live_resources(QRhiImplementation& rhi)
{
  return rhi.activeResources();
}

inline QRhiResource* live_resource(LiveResources::const_iterator it)
{
  return *it;
}
#endif

inline const auto* binding_data(const QRhiShaderResourceBinding& b)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  return QRhiImplementation::shaderResourceBindingData(b);
#else
  return b.data();
#endif
}
}
}
