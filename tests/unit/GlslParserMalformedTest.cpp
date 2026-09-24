// Geometry filters go through the GLSL parser (libisf's
// extract_glsl_function_definitions). On malformed GLSL, bison's error
// recovery drops IDENTIFIER tokens whose string the lexer strdup()ed; the
// grammar's %destructor frees them. The leak check is LeakSanitizer's, in the
// ASan build; elsewhere this only checks that the parse does not crash.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("a geometry filter with malformed GLSL is parsed without leaking", "[isf][glsl]")
{
  const std::string header = R"_(/*{
  "ISFVSN": "2",
  "MODE": "GEOMETRY_FILTER",
  "INPUTS": [ { "NAME": "shift", "TYPE": "float", "DEFAULT": 0.0 } ]
}*/
)_";
  for(const char* body :
      {"void process_vertex(inout vec3 position) { int x y z; }",
       "uniform vec4 a b c d;\nvoid process_vertex(inout vec3 position) { }",
       "void f( foo bar baz { }\nvoid process_vertex(inout vec3 position) { }"})
  {
    CAPTURE(body);
    try
    {
      isf::parser p{header + body, isf::parser::ShaderType::GeometryFilter};
    }
    catch(...)
    {
    }
    SUCCEED();
  }
}
