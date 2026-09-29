// A Faust process keeps the folder its import() statements resolve against.
// However the document it was loaded from wrote that folder, it is saved the
// portable way: <LIBRARY>: or <PROJECT>:-relative when it lives there.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <QSettings>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "A Faust import folder in the library is saved library-relative",
    "[integration][faust][serialization]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir libraryDir;
    REQUIRE(libraryDir.isValid());
    const QString library = score::test::canonical(libraryDir.path());
    score::test::write_file(library + "/faust/helpers.lib", "gain = *(0.5);\n");

    struct LibraryRoot
    {
      QVariant previous = QSettings{}.value("Library/RootPath");
      explicit LibraryRoot(const QString& root)
      {
        QSettings{}.setValue("Library/RootPath", root);
      }
      ~LibraryRoot() { QSettings{}.setValue("Library/RootPath", previous); }
    } root{library};

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    if(!score::test::add_process(
           *doc, QStringLiteral("5354c61a-1649-4f59-b952-5c2f1b79c1bd"),
           QStringLiteral("process = _;")))
      SKIP("the Faust plug-in is not in this build");

    // A document that wrote the folder as an absolute path.
    QByteArray json = score::test::save_as_json(*doc);
    REQUIRE(json.contains(R"("Path":"")"));
    json.replace(R"("Path":"")", QString{R"("Path":")" + library + R"(/faust")"}.toUtf8());

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto* loaded = ctx.docManager.loadDocument(
        ctx, QStringLiteral("absolute-faust-path"), json, JSONObject::type(),
        *delegates.begin());
    REQUIRE(loaded != nullptr);

    const QByteArray saved = score::test::save_as_json(*loaded);
    CHECK(saved.contains(R"("Path":"<LIBRARY>:faust")"));
    CHECK_FALSE(saved.contains(library.toUtf8()));
  });
}

TEST_CASE(
    "A Faust import folder in the project follows the document through Save as",
    "[integration][faust][serialization]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir projectDir, otherDir;
    REQUIRE(projectDir.isValid());
    REQUIRE(otherDir.isValid());
    const QString project = score::test::canonical(projectDir.path());
    const QString other = score::test::canonical(otherDir.path());
    score::test::write_file(project + "/faust/effect.dsp", "process = _;\n");

    auto* doc = score::test::project_document(ctx, project);
    if(!score::test::add_process(
           *doc, QStringLiteral("5354c61a-1649-4f59-b952-5c2f1b79c1bd"),
           project + "/faust/effect.dsp"))
      SKIP("the Faust plug-in is not in this build");

    const QByteArray json = score::test::save_as_json(*doc);
    REQUIRE(json.contains(R"("Path":"<PROJECT>:faust")"));

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    const auto load = [&](const QByteArray& data, SerializationIdentifier format) {
      return ctx.docManager.loadDocument(
          ctx, project + "/project.score", data, format, *delegates.begin());
    };

    // Binary: written the same way, read back to the same folder.
    auto* fromBinary = load(doc->saveAsByteArray(), DataStream::type());
    REQUIRE(fromBinary != nullptr);
    CHECK(score::test::save_as_json(*fromBinary).contains(R"("Path":"<PROJECT>:faust")"));

    // Saved elsewhere, the folder stays where it is.
    auto* loaded = load(json, JSONObject::type());
    REQUIRE(loaded != nullptr);
    REQUIRE(ctx.docManager.saveDocumentAs(*loaded, other + "/moved.score"));
    const QByteArray moved = score::test::save_as_json(*loaded);
    CHECK(moved.contains(QString{R"("Path":")" + project + R"(/faust")"}.toUtf8()));
  });
}
