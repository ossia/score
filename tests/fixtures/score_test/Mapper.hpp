#pragma once

// Real Mapper devices, using the same Score scripting entry point as the UI.
// Protocol tests observe the resulting device tree, not a stand-alone QJSEngine.
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/Qml/EditContext.hpp>

#include <core/document/Document.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QThread>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace score::test::mapper
{
struct fixture
{
  const score::GUIApplicationContext& ctx;
  score::Document& doc;
  QQmlEngine engine;

  fixture(const score::GUIApplicationContext& c, score::Document& d)
      : ctx{c}, doc{d}
  {
    engine.globalObject().setProperty("Score", engine.newQObject(new JS::EditJsContext));
  }

  QJSValue eval(const QString& js)
  {
    auto res = engine.evaluate(js);
    if(res.isError())
      FAIL("script failed: " << res.toString().toStdString()
                             << "\nscript was: " << js.toStdString());
    return res;
  }

  void createMapper(const QString& name, const QString& qml)
  {
    const auto settings
        = QJsonDocument{QJsonObject{{"Text", qml}}}.toJson(QJsonDocument::Compact);
    auto create = eval(QStringLiteral(
        "(function(name, settings) { Score.createDevice(name, "
        "'910e2d87-a087-430d-b725-c988fe2bea01', settings); })"));
    auto result = create.call({name, engine.toScriptValue(
        QJsonDocument::fromJson(settings).object().toVariantMap())});
    if(result.isError())
      FAIL("Mapper creation failed: " << result.toString().toStdString());
  }

  void removeMapper(const QString& name)
  {
    auto remove = eval(QStringLiteral("(function(name) { Score.removeDevice(name); })"));
    auto result = remove.call({name});
    if(result.isError())
      FAIL("Mapper removal failed: " << result.toString().toStdString());
  }

  QVariantMap contents(const QString& name)
  {
    if(!m_iterate.isCallable())
      m_iterate = eval(QStringLiteral(R"js(
        (function(name) {
          var res = {};
          Score.iterateDevice(name, function(addr, v) { res[addr] = v.value; });
          return res;
        })
      )js"));
    auto res = m_iterate.call({name});
    if(res.isError())
      FAIL("iterateDevice failed: " << res.toString().toStdString());
    return res.toVariant().toMap();
  }

  void push(const QString& name, const QString& path, const ossia::value& value)
  {
    auto& devices = doc.context().plugin<Explorer::DeviceDocumentPlugin>().list();
    auto* device = devices.findDevice(name);
    REQUIRE(device);
    REQUIRE(device->getDevice());
    auto* node = ossia::net::find_node(device->getDevice()->get_root_node(), path.toStdString());
    INFO("Mapper address: " << name.toStdString() << ":" << path.toStdString());
    REQUIRE(node);
    REQUIRE(node->get_parameter());
    node->get_parameter()->push_value(value);
  }

  static QString script(const QString& filename)
  {
    QFile file{QStringLiteral(SCORE_TEST_QML_PROTOCOL_DIR) + '/' + filename};
    INFO("QML fixture: " << file.fileName().toStdString());
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QString::fromUtf8(file.readAll());
  }

  template <typename F>
  static bool spin(F pred, int ms = 5000)
  {
    QElapsedTimer timer;
    timer.start();
    do
    {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
      if(pred())
        return true;
      QThread::msleep(2);
    } while(timer.elapsed() < ms);
    return pred();
  }

private:
  QJSValue m_iterate;
};
}
