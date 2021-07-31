#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPluginBase.hpp>
#include <ossia-qt/time_value.hpp>
#include <score_plugin_transport_export.h>

namespace Transport
{
class SCORE_PLUGIN_TRANSPORT_EXPORT DocumentPlugin
    : public score::DocumentPlugin
{
  W_OBJECT(DocumentPlugin)
public:
  DocumentPlugin(
      const score::DocumentContext& ctx,
      QObject* parent);

  ~DocumentPlugin();

  void play()
      E_SIGNAL(SCORE_PLUGIN_TRANSPORT_EXPORT, play)
  void pause()
      E_SIGNAL(SCORE_PLUGIN_TRANSPORT_EXPORT, pause)
  void stop()
      E_SIGNAL(SCORE_PLUGIN_TRANSPORT_EXPORT, stop)
  void transport(ossia::time_value t)
      E_SIGNAL(SCORE_PLUGIN_TRANSPORT_EXPORT, transport, t)
};
}
