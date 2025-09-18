#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

#include <thread>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}
class QQuickWindow;
namespace JS
{
class ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& ctx);

  void on_createdDocument(score::Document& doc) override;

  ~ApplicationPlugin() override;
  void afterStartup() override;

  QQmlEngine m_engine;
  QQmlEngine m_dummyEngine;
  QQmlComponent* m_comp{};
  QQuickWindow* m_window{};

  std::atomic_bool m_processMessages{};
  std::thread m_asioThread;
  ossia::net::network_context_ptr m_asioContext;
};
}
