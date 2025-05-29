#pragma once

#include <Process/CodeWriter.hpp>

#include <boost/core/demangle.hpp>

namespace Crousti
{

template <typename Info>
struct CodeWriter : Process::AvndCodeWriter
{
  using Process::AvndCodeWriter::AvndCodeWriter;

  std::string typeName() const noexcept override
  {
    return boost::core::demangle(typeid(Info).name());
  }
};
}
