#include <JS/Qml/ImportedUi.hpp>

#include <QEventLoop>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTimer>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <memory>

TEST_CASE(
    "Imported UI rejects unsupported nested values without replacing inputs",
    "[js][imported-ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QJSEngine engine;
    JS::ImportedUi host;
    const QVariantMap original{{QStringLiteral("level"), 42}};
    host.setValues(original);

    const auto unsupported = engine.evaluate("({ nested: [function() {}] })");
    host.setValues({{QStringLiteral("payload"), QVariant::fromValue(unsupported)}});
    CHECK(host.values() == original);
    CHECK(host.status() == QStringLiteral("error"));
    CHECK_FALSE(host.errorString().isEmpty());

    const QVariantMap corrected{{QStringLiteral("level"), 43}};
    host.setValues(corrected);
    CHECK(host.values() == corrected);
  });
}

TEST_CASE(
    "Imported UI dispatches shared-signal outputs before applying feedback",
    "[js][imported-ui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    QFile fixture{directory.filePath("content.qml")};
    REQUIRE(fixture.open(QIODevice::WriteOnly));
    const QByteArray content = R"(
import QtQuick
Item {
  objectName: "ImportedRoot"
  width: 200; height: 100
  property int a: 0
  property int b: 0
  signal commit()
})";
    REQUIRE(fixture.write(content) == content.size());
    fixture.close();

    qmlRegisterType<JS::ImportedUi>("ImportedUiTests", 1, 0, "ImportedUi");
    QQmlEngine engine;
    QQmlComponent component{&engine};
    component.setData(
        "import QtQuick\nimport ImportedUiTests 1.0\nImportedUi { width: 200; height: "
        "100 }",
        QUrl{});
    QVariantList mapping;
    for(const auto& name : {QStringLiteral("a"), QStringLiteral("b")})
      mapping.push_back(
          QVariantMap{
              {"name", name},
              {"object", "."},
              {"property", name},
              {"signal", "commit"},
              {"direction", "both"}});
    std::unique_ptr<QObject> object{component.createWithInitialProperties(
        {{"source", QUrl::fromLocalFile(fixture.fileName())},
         {"mapping", mapping},
         {"values", QVariantMap{{"a", 0}, {"b", 0}}}})};
    REQUIRE(object != nullptr);
    auto* host = qobject_cast<JS::ImportedUi*>(object.get());
    REQUIRE(host != nullptr);
    QEventLoop loading;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loading, &QEventLoop::quit);
    QObject::connect(host, &JS::ImportedUi::statusChanged, &loading, [&] {
      if(host->status() == "ready" || host->status() == "error")
        loading.quit();
    });
    timeout.start(5000);
    if(host->status() != "ready" && host->status() != "error")
      loading.exec();
    INFO(host->errorString().toStdString());
    REQUIRE(host->status() == "ready");
    auto* root = host->findChild<QQuickItem*>("ImportedRoot");
    REQUIRE(root != nullptr);

    QVariantMap outputs;
    QObject::connect(
        host, &JS::ImportedUi::event, host,
        [&](const QString& name, const QVariant& value) {
      outputs[name] = value;
      auto feedback = host->values();
      feedback[name] = value;
      host->setValues(feedback);
      CHECK(root->property("b").toInt() == 2);
    });
    root->setProperty("a", 1);
    root->setProperty("b", 2);
    REQUIRE(QMetaObject::invokeMethod(root, "commit", Qt::DirectConnection));
    const QVariantMap expected{{"a", 1}, {"b", 2}};
    CHECK(outputs == expected);
    CHECK(host->values() == expected);
    CHECK(root->property("b").toInt() == 2);
  });
}
