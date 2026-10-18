// PERSISTENT keeps the previous frame of a storage image or buffer as
// <name>_prev, which the shader reads while it updates <name>: only
// read_write has those semantics. write_only was already rejected (nothing
// would read the history); read_only was accepted and even recommended by
// that error, and generated a _prev binding the renderer never fed (shader
// board U47). Both must be refused with a message pointing at read_write.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

namespace
{
std::string csf(const std::string& resource)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ )_"
         + resource + R"_( ],
  "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main() { }
)_";
}

std::string image(const std::string& access)
{
  return R"_({ "NAME": "state", "TYPE": "image", "FORMAT": "rgba8", "WIDTH": "4", "HEIGHT": "4", "PERSISTENT": true)_"
         + (access.empty() ? std::string{} : ", \"ACCESS\": \"" + access + "\"") + " }";
}

std::string buffer(const std::string& access)
{
  return R"_({ "NAME": "state", "TYPE": "storage", "PERSISTENT": true, "LAYOUT": [ { "NAME": "v", "TYPE": "float[4]" } ])_"
         + (access.empty() ? std::string{} : ", \"ACCESS\": \"" + access + "\"") + " }";
}
}

TEST_CASE(
    "PERSISTENT storage images and buffers must be read_write", "[isf][csf][persistent][U47]")
{
  const bool is_image = GENERATE(true, false);
  const auto access = GENERATE(std::string{"read_only"}, std::string{"write_only"});
  CAPTURE(is_image, access);
  const auto src = csf(is_image ? image(access) : buffer(access));
  CHECK_THROWS_WITH(
      (isf::parser{{}, src, 450, isf::parser::ShaderType::CSF}),
      Catch::Matchers::ContainsSubstring("read_write")
          && !Catch::Matchers::ContainsSubstring("or read_only"));
}

TEST_CASE(
    "PERSISTENT read_write (or the read_write default) storage parses",
    "[isf][csf][persistent][U47]")
{
  const bool is_image = GENERATE(true, false);
  const auto access = GENERATE(std::string{"read_write"}, std::string{});
  CAPTURE(is_image, access);
  const auto src = csf(is_image ? image(access) : buffer(access));
  CHECK_NOTHROW(isf::parser{{}, src, 450, isf::parser::ShaderType::CSF});
}
