// Header keys and keyword values are case-independent. Root keys and TYPE were
// already folded, but nested keys (NAME, LABEL, LAYOUT, PIPELINE_STATE
// members...) had to be uppercase, ACCESS values lowercase and execution-model
// TYPEs uppercase (shader board U02).
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("ISF nested keys are case-independent", "[gfx][isf][U02]")
{
  const std::string src = R"_(/*{
  "isfvsn": "2.0",
  "inputs": [ { "name": "gain", "type": "float", "default": 0.25, "min": 0.0, "Max": 2.0, "label": "Gain" } ],
  "pipeline_state": { "depth_test": true, "Cull_Mode": "back" }
}*/
void main() { gl_FragColor = vec4(gain); }
)_";
  ::isf::parser p{{}, src, 450, ::isf::parser::ShaderType::ISF};
  const auto& d = p.data();
  REQUIRE(d.inputs.size() == 1);
  CHECK(d.inputs[0].name == "gain");
  CHECK(d.inputs[0].label == "Gain");
  const auto* f = ossia::get_if<isf::float_input>(&d.inputs[0].data);
  REQUIRE(f);
  CHECK(f->def == 0.25);
  CHECK(f->max == 2.0);
  CHECK(d.default_state.depth_test.value_or(false));
  CHECK(d.default_state.cull_mode.value_or("") == "back");
}

TEST_CASE("CSF keys and keyword values are case-independent", "[gfx][csf][U02]")
{
  const std::string src = R"_(/*{
  "IsfVsn": "2.0",
  "Mode": "compute_shader",
  "Resources": [
    { "Name": "buf", "Type": "Storage", "Access": "READ_WRITE", "Layout": [ { "Name": "v", "Type": "float[]" } ] },
    { "name": "outputImage", "type": "image", "access": "Write_Only", "format": "RGBA8", "width": "8", "height": "8" }
  ],
  "Passes": [
    { "Local_Size": [8, 8, 1], "Execution_Model": { "Type": "2d_image", "Target": "outputImage" } },
    { "local_size": [64, 1, 1], "execution_model": { "type": "manual", "workgroups": [2, 1, 1] } }
  ]
}*/
void main() { }
)_";
  ::isf::parser p{{}, src, 450, ::isf::parser::ShaderType::CSF};
  const auto& d = p.data();
  REQUIRE(d.inputs.size() == 2);
  const auto* st = ossia::get_if<isf::storage_input>(&d.inputs[0].data);
  REQUIRE(st);
  CHECK(st->access == "read_write");
  REQUIRE(st->layout.size() == 1);
  CHECK(st->layout[0].name == "v");
  const auto* img = ossia::get_if<isf::csf_image_input>(&d.inputs[1].data);
  REQUIRE(img);
  CHECK(img->access == "write_only");
  REQUIRE(d.csf_passes.size() == 2);
  CHECK(d.csf_passes[0].execution_type == "2D_IMAGE");
  CHECK(d.csf_passes[0].target_resource == "outputImage");
  CHECK(d.csf_passes[1].execution_type == "MANUAL");
  CHECK(d.csf_passes[1].workgroups[0] == 2);
  CHECK(p.compute_shader().find("outputImage") != std::string::npos);
}
