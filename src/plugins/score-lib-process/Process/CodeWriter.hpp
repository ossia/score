#pragma once
#include <score/model/Identifier.hpp>

#include <score_lib_process_export.h>

#include <string>
namespace Process
{
class ProcessModel;
class Port;
class Inlet;
class ControlInlet;
class Outlet;

class SCORE_LIB_PROCESS_EXPORT CodeWriter
{
public:
  const ProcessModel& self;
  explicit CodeWriter(const ProcessModel& p) noexcept;
  virtual ~CodeWriter();

  CodeWriter() = delete;
  CodeWriter(const CodeWriter&) = delete;
  CodeWriter(CodeWriter&&) = delete;
  CodeWriter& operator=(const CodeWriter&) = delete;
  CodeWriter& operator=(CodeWriter&&) = delete;

  std::string variable;

  virtual std::string typeName() const noexcept = 0;
  virtual std::string initializer() const noexcept = 0;
  virtual std::string accessInlet(const Id<Process::Port>& id) const noexcept = 0;
  virtual std::string accessOutlet(const Id<Process::Port>& id) const noexcept = 0;
  virtual std::string execute() const noexcept = 0;
};

class SCORE_LIB_PROCESS_EXPORT DummyCodeWriter : public Process::CodeWriter
{
public:
  using Process::CodeWriter::CodeWriter;

  std::string typeName() const noexcept override;
  std::string initializer() const noexcept override;
  std::string accessInlet(const Id<Process::Port>& id) const noexcept override;
  std::string accessOutlet(const Id<Process::Port>& id) const noexcept override;
  std::string execute() const noexcept override;
};

struct SCORE_LIB_PROCESS_EXPORT AvndCodeWriter : Process::CodeWriter
{
  using Process::CodeWriter::CodeWriter;

  std::string initializer() const noexcept override;

  std::string accessInlet(const Id<Process::Port>& id) const noexcept override;

  std::string accessOutlet(const Id<Process::Port>& id) const noexcept override;

  std::string execute() const noexcept override;
};
}
