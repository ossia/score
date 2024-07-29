#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

class QQuickWindow;
namespace JS
{
class ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& ctx);

  ~ApplicationPlugin() override;
  bool handleStartup() override;

  QQmlEngine m_engine;
  QQmlComponent* m_comp{};
  QQuickWindow* m_window{};
};
}
