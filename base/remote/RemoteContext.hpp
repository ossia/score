#pragma once
#include <QList>
#include <vector>
#include <memory>

class QQmlApplicationEngine;
class QQuickItem;
namespace ossia::net { class generic_device; }
namespace RemoteUI
{
class WebSocketClient;
class NodeModel;
class WidgetListData;
class RemoteApplication;

struct Context
{
  Context(
      QQmlApplicationEngine& e,
      const QList<RemoteUI::WidgetListData*>& w,
      NodeModel& n,
      std::vector<std::unique_ptr<ossia::net::generic_device>>& d,
      RemoteApplication& app,
      QQuickItem* i)
      : engine{e}
      , widgets{w}
      , nodes{n}
      , device{d}
      , application{app}
      , centralItem{i}
  {
  }
  QQmlApplicationEngine& engine;
  const QList<RemoteUI::WidgetListData*>& widgets;
  NodeModel& nodes;
  std::vector<std::unique_ptr<ossia::net::generic_device>>& device;
  RemoteApplication& application;
  QQuickItem* centralItem{};
};
}
