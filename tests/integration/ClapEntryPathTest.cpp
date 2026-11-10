// A CLAP library's entry is initialised with the path clap/entry.h asks for:
// the library itself on Linux and Windows, the bundle holding it on macOS,
// where score keeps the path of the binary in Contents/MacOS. clap-init-path
// is a bundle on macOS.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

#include <string>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");
}

TEST_CASE("CLAP: the entry is initialised with its library's path", "[integration][clap]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, clap_uuid, QStringLiteral(CLAP_INIT_PATH_BINARY ":::io.ossia.test.init-path"));
    REQUIRE(proc);

    void* lib = dlopen(CLAP_INIT_PATH_BINARY, RTLD_LAZY | RTLD_NOLOAD);
    REQUIRE(lib);
    auto seen = reinterpret_cast<const char* (*)()>(dlsym(lib, "clap_init_path_seen"));
    REQUIRE(seen);
    CHECK(std::string(seen()) == CLAP_INIT_PATH_EXPECTED);
    dlclose(lib);
  });
}
