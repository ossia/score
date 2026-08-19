// score::try_load_library and the optional-dependency loaders built on it.
//
// ossia::dylib_loader reports a missing library by throwing from its
// constructor. The loaders for gphoto2, GStreamer and X11 wrapped that in a
// function-try-block on their own constructor, which rethrows when the handler
// falls off its end: instead of leaving their `available` flag false, they
// threw out of instance() and took the application down whenever the library
// was not installed.

#include <score/tools/DynamicLibrary.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// A library name no system can resolve.
constexpr auto missing = "libscore-definitely-not-installed.so.999";

//! The shape the optional-dependency loaders use.
struct loader
{
  bool available{};

  static const loader& instance() noexcept
  {
    static const loader self;
    return self;
  }

private:
  loader()
  {
    m_lib = score::try_load_library(missing);
    if(!m_lib)
      return;

    available = true;
  }

  std::optional<ossia::dylib_loader> m_lib;
};
}

TEST_CASE("A missing library is reported, not thrown", "[dylib]")
{
  REQUIRE(!score::try_load_library(missing).has_value());
  REQUIRE(!score::try_load_library({missing, "libscore-not-there-either.so"})
               .has_value());
  REQUIRE(!score::try_load_library(std::vector<std::string_view>{}).has_value());
}

TEST_CASE("An installed library is loaded and its symbols resolve", "[dylib]")
{
#if defined(_WIN32)
  auto lib = score::try_load_library("kernel32.dll");
  const char* symbol = "GetTickCount";
#elif defined(__APPLE__)
  auto lib = score::try_load_library({"libSystem.B.dylib", "libSystem.dylib"});
  const char* symbol = "getpid";
#else
  auto lib = score::try_load_library({"libm.so.6", "libm.so"});
  const char* symbol = "floor";
#endif

  REQUIRE(lib.has_value());
  REQUIRE(lib->symbol<void*>(symbol) != nullptr);
  REQUIRE(lib->symbol<void*>("score_no_such_symbol_here") == nullptr);
}

// The regression: instance() must hand back an unavailable loader rather than
// propagate the failure to load.
TEST_CASE("A loader whose library is absent stays unavailable", "[dylib]")
{
  const loader* l{};
  REQUIRE_NOTHROW(l = &loader::instance());
  REQUIRE(!l->available);

  // The failure is cached with the singleton: asking again must not throw either
  REQUIRE_NOTHROW(loader::instance());
}
