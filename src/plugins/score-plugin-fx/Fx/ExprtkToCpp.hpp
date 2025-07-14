#pragma once
#include <string>
#include <Process/CodeWriter.hpp>

namespace Nodes
{
std::string exprtk_to_cpp(std::string exprtk, bool optional) noexcept;

struct MathMappingCodeWriter : public Process::CodeWriter
{
  explicit MathMappingCodeWriter(const Process::ProcessModel& p) noexcept;

  std::string initializer() const noexcept override;
  std::string typeName() const noexcept override;
  std::string accessInlet(const Id<Process::Port>& id) const noexcept override;
  std::string accessOutlet(const Id<Process::Port>& id) const noexcept override;
  std::string execute() const noexcept override;

  bool in_vector{}, ret_vector{};
};
}
