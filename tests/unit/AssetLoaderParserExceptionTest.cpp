// A parser that throws while reading an asset -- a file declaring counts that
// exhaust memory ends in std::bad_alloc -- is a rejected asset, not an
// exception out of AssetLoader::ins::asset_t::process(), which the avnd runtime
// calls on the GUI thread without a handler.
#include <Threedim/AssetLoader.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <new>
#include <stdexcept>

namespace
{
std::shared_ptr<const ossia::scene_state> throwsBadAlloc(const halp::text_file_view&)
{
  throw std::bad_alloc{};
}

std::shared_ptr<const ossia::scene_state> throwsLengthError(const halp::text_file_view&)
{
  throw std::length_error{"vector"};
}

bool rejected(std::string_view filename)
{
  halp::text_file_view tv;
  tv.filename = filename;
  tv.bytes = "not an asset";
  return !Threedim::AssetLoader::ins::asset_t::process(tv);
}
}

TEST_CASE("a parser throwing bad_alloc rejects the asset", "[threedim][asset]")
{
  Threedim::AssetLoaderRegistry::register_parser("scoretestbadalloc", &throwsBadAlloc);
  CHECK_NOTHROW(rejected("/nonexistent/model.scoretestbadalloc"));
  CHECK(rejected("/nonexistent/model.scoretestbadalloc"));
}

TEST_CASE("a parser throwing length_error rejects the asset", "[threedim][asset]")
{
  Threedim::AssetLoaderRegistry::register_parser("scoretestlength", &throwsLengthError);
  CHECK_NOTHROW(rejected("/nonexistent/model.scoretestlength"));
  CHECK(rejected("/nonexistent/model.scoretestlength"));
}
