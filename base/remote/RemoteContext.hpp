#pragma once
#include <QList>
class QQmlApplicationEngine;
class QQuickItem;
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
      const NodeModel& n,
      WebSocketClient& ws,
      RemoteApplication& app,
      QQuickItem* i)
      : engine{e}
      , widgets{w}
      , nodes{n}
      , websocket{ws}
      , application{app}
      , centralItem{i}
  {
  }
  QQmlApplicationEngine& engine;
  const QList<RemoteUI::WidgetListData*>& widgets;
  const NodeModel& nodes;
  WebSocketClient& websocket;
  RemoteApplication& application;
  QQuickItem* centralItem{};
};
}
