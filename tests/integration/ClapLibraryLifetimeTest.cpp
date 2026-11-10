// On macOS a CLAP library stays loaded once its last process is gone: Cocoa
// keeps references into the image of a plug-in that showed an editor (its
// Objective-C classes, in weak tables and notification observers) past the
// plug-in's life, and dereferences them when another window goes away.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");

bool loaded(const char* path)
{
  void* lib = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
  if(lib)
    dlclose(lib);
  return lib != nullptr;
}
}

TEST_CASE(
    "CLAP: a library stays loaded once its processes are gone",
    "[integration][clap][macos]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    REQUIRE(!loaded(CLAP_BLOCK_RECORDER_PATH));

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, clap_uuid,
        QStringLiteral(CLAP_BLOCK_RECORDER_PATH ":::io.ossia.test.block-recorder"));
    REQUIRE(proc);
    CHECK(loaded(CLAP_BLOCK_RECORDER_PATH));

    ctx.docManager.forceCloseDocument(ctx, *doc);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(loaded(CLAP_BLOCK_RECORDER_PATH));
  });
}
