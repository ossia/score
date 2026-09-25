// A JSON-like message handed to a script keeps its lists as arrays: scripts
// test Array.isArray on them (rect-mapper's shapes).
#include <JS/Qml/VariantToJs.hpp>

#include <QCoreApplication>
#include <QJSEngine>
#include <QVariantList>
#include <QVariantMap>

#include <catch2/catch_test_macros.hpp>

namespace
{
bool isArray(QJSEngine& e, const QJSValue& v, const QString& path)
{
  auto f = e.evaluate(QStringLiteral("(function(x) { return Array.isArray(x.%1); })").arg(path));
  return f.call({v}).toBool();
}
}

TEST_CASE("Lists nested in a message are arrays in the script", "[unit][js]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QCoreApplication app{argc, argv};
  QJSEngine e;
  QVariantMap shape{{"vertices", QVariantList{QVariantList{0.1, 0.2}, QVariantList{0.3, 0.4}}}};
  QVariantMap msg{{"type", "updateRects"}, {"rects", QVariantList{shape}}, {"showOverlay", true}};

  const auto v = JS::variantToJs(e, msg);
  CHECK(isArray(e, v, "rects"));
  CHECK(isArray(e, v, "rects[0].vertices"));
  CHECK(isArray(e, v, "rects[0].vertices[1]"));
  CHECK(v.property("rects").property(0).property("vertices").property(1).property(1).toNumber() == 0.4);
  CHECK(v.property("showOverlay").toBool());
  CHECK(v.property("type").toString() == "updateRects");

  SECTION("a value from another engine")
  {
    QJSEngine other;
    auto obj = other.evaluate(QStringLiteral("({ rects: [ { a: [1, 2] } ] })"));
    const auto w = JS::variantToJs(e, QVariant::fromValue(obj));
    CHECK(isArray(e, w, "rects"));
    CHECK(isArray(e, w, "rects[0].a"));
  }
}
