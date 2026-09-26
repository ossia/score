// View.grabScene() captures what the scenario view shows, not a fixed scene
// region, into an SVG that carries its size.
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <JS/Qml/ViewContext.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QWidget>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("View.grabScene captures the visible part of the scenario", "[integration][js]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    auto doc = score::test::new_document(app);
    REQUIRE(doc);
    if(auto w = app.mainWindow)
    {
      w->resize(1280, 800);
      w->show();
    }
    for(int i = 0; i < 10; i++)
      QApplication::processEvents();

    auto view = qobject_cast<Scenario::ScenarioDocumentView*>(
        &doc->view()->viewDelegate());
    REQUIRE(view);
    const auto visible = view->visibleSceneRect();
    REQUIRE(visible.width() > 10);
    REQUIRE(visible.height() > 10);

    QTemporaryDir dir;
    JS::JsViewContext js;

    const auto png = dir.filePath("scene.png");
    REQUIRE(js.grabScene(png));
    QImage img{png};
    REQUIRE(!img.isNull());
    CHECK(img.size() == visible.size().toSize());

    const auto svg = dir.filePath("scene.svg");
    REQUIRE(js.grabScene(svg));
    QFile f{svg};
    REQUIRE(f.open(QIODevice::ReadOnly));
    const auto text = f.readAll();
    CHECK(text.contains(
        QStringLiteral("viewBox=\"0 0 %1 %2\"")
            .arg(visible.size().toSize().width())
            .arg(visible.size().toSize().height())
            .toUtf8()));
  });
}
