#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/network/value/value.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <bit>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace avnd_tools
{
// Byte limits bound both the string and the number of integer list elements.
// Inlets retain ossia::value so host conversion cannot silently coerce floats,
// booleans or strings into integer bytes. Errors never publish partial data;
// successful messages clear Error before publishing the converted value.
struct StringToBytes
{
  halp_meta(name, "String to Bytes")
  halp_meta(c_name, "avnd_string_to_bytes")
  halp_meta(category, "Control/Strings")
  halp_meta(author, "ossia team")
  halp_meta(uuid, "ab157b25-7233-46bb-a134-92547242bf09")
  halp_meta(
      description,
      "Convert raw string bytes to integers from 0 to 255, preserving NUL and arbitrary "
      "binary data without UTF-8 validation.")

  struct ins
  {
    struct : halp::val_port<"String", ossia::value>
    {
      halp_meta(
          description,
          "Raw string bytes, not Unicode codepoints. Only string values are accepted.")
      void update(StringToBytes& self) { self.process(); }
    } input;
    struct : halp::spinbox_i32<"Max bytes", halp::range{1, 1048576, 1048576}>
    {
      halp_meta(
          description,
          "Maximum input bytes and output integer count; hard ceiling 1048576. Changing "
          "this control does not replay input.")
    } max_bytes;
  } inputs;
  struct outs
  {
    struct : halp::callback<"Bytes", std::vector<int>>
    {
      halp_meta(description, "One integer from 0 to 255 for each raw input byte.")
    } bytes;
    halp::callback<"Error", std::string> error;
  } outputs;

  void process()
  {
    const auto fail = [this](const char* message) { outputs.error(message); };
    if(inputs.max_bytes < 1 || inputs.max_bytes > 1048576)
      return fail("Max bytes must be between 1 and 1048576");
    const auto* text = inputs.input.value.target<std::string>();
    if(!text)
      return fail("Input must be a string");
    if(text->size() > std::size_t(inputs.max_bytes.value))
      return fail("Input exceeds byte limit");
    std::vector<int> bytes;
    bytes.reserve(text->size());
    for(unsigned char byte : *text)
      bytes.push_back(byte);
    outputs.error(std::string{});
    outputs.bytes(std::move(bytes));
  }
};

struct BytesToString
{
  halp_meta(name, "Bytes to String")
  halp_meta(c_name, "avnd_bytes_to_string")
  halp_meta(category, "Control/Strings")
  halp_meta(author, "ossia team")
  halp_meta(uuid, "f398ff8f-1836-4d49-80a4-d786c12d13c6")
  halp_meta(
      description,
      "Convert an integer byte list to a raw string, preserving NUL and arbitrary "
      "binary data. Rejects nonintegers and values outside 0 to 255; no UTF-8 "
      "validation.")

  struct ins
  {
    struct : halp::val_port<"Bytes", ossia::value>
    {
      halp_meta(
          description,
          "A list containing only integers from 0 to 255; floats, booleans, strings and "
          "nested lists are errors.")
      void update(BytesToString& self) { self.process(); }
    } input;
    struct : halp::spinbox_i32<"Max bytes", halp::range{1, 1048576, 1048576}>
    {
      halp_meta(
          description,
          "Maximum input integer count and output bytes; hard ceiling 1048576. Changing "
          "this control does not replay input.")
    } max_bytes;
  } inputs;
  struct outs
  {
    struct : halp::callback<"String", std::string>
    {
      halp_meta(
          description, "Raw string bytes, including NUL and invalid UTF-8 sequences.")
    } text;
    halp::callback<"Error", std::string> error;
  } outputs;

  void process()
  {
    const auto fail = [this](const char* message) { outputs.error(message); };
    if(inputs.max_bytes < 1 || inputs.max_bytes > 1048576)
      return fail("Max bytes must be between 1 and 1048576");
    const auto* bytes = inputs.input.value.target<std::vector<ossia::value>>();
    if(!bytes)
      return fail("Input must be a list of integer bytes");
    if(bytes->size() > std::size_t(inputs.max_bytes.value))
      return fail("Input exceeds byte limit");
    // Validate before allocating the destination or publishing any bytes.
    for(const auto& byte : *bytes)
    {
      const auto* integer = byte.target<int>();
      if(!integer || *integer < 0 || *integer > 255)
        return fail("Every byte must be an integer between 0 and 255");
    }
    std::string text(bytes->size(), '\0');
    for(std::size_t i = 0; i < bytes->size(); ++i)
      text[i] = std::bit_cast<char>(static_cast<unsigned char>((*bytes)[i].get<int>()));
    outputs.error(std::string{});
    outputs.text(std::move(text));
  }
};
}
