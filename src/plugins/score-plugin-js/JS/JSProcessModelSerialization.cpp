// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSProcessModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/FilePath.hpp>

#include <QString>

template <>
SCORE_PLUGIN_JS_EXPORT void DataStreamReader::read(const JS::QmlSource& p)
{
  m_stream << p.execution << p.ui;
}

template <>
SCORE_PLUGIN_JS_EXPORT void DataStreamWriter::write(JS::QmlSource& p)
{
  m_stream >> p.execution >> p.ui;
}

template <>
void DataStreamReader::read(const JS::ProcessModel& proc)
{
  auto& ctx = score::IDocument::documentContext(proc);
  // A script still identical to the .qml it came from travels as that path
  // alone: embedding a copy would freeze the document on today's version of a
  // library file the user expects to keep receiving updates for.
  m_stream << (proc.followsRootFile() ? JS::QmlSource{} : proc.m_program) << proc.m_state
           << score::relativizeFilePath(proc.m_root, ctx);

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(JS::ProcessModel& proc)
{
  JS::QmlSource str;
  JS::JSState st;
  m_stream >> str >> st >> proc.m_root;
  if(!proc.m_root.isEmpty())
  {
    auto& ctx = score::IDocument::documentContext(proc);
    proc.m_root = score::locateFilePath(proc.m_root, ctx);
    if(str.execution.isEmpty())
      str = JS::ProcessModel::readProgramFromFile(proc.m_root);
  }
  proc.setState(st);
  (void)proc.setProgram(str);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const JS::ProcessModel& proc)
{
  // A script still identical to the .qml it came from travels as that path
  // alone: embedding a copy would freeze the document on today's version of a
  // library file the user expects to keep receiving updates for.
  if(!proc.followsRootFile())
  {
    obj["Script"] = proc.program().execution;
    if(const auto& ui = proc.program().ui; !ui.isEmpty())
      obj["Ui"] = ui;
  }
  if(const auto& st = proc.state(); !st.empty())
    obj["State"] = st;
  if(const auto& r = proc.m_root; !r.isEmpty())
  {
    auto& ctx = score::IDocument::documentContext(proc);
    obj["Root"] = score::relativizeFilePath(proc.m_root, ctx);
  }
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(JS::ProcessModel& proc)
{
  JS::QmlSource p;
  JS::JSState st;
  if(auto script = obj.tryGet("Script"))
    p.execution = script->toString();

  if(auto ui = obj.tryGet("Ui"))
    p.ui = ui->toString();

  if(auto json_st = obj.tryGet("State"))
    st <<= *json_st;

  if(auto json_r = obj.tryGet("Root"))
  {
    proc.m_root <<= *json_r;
    if(!proc.m_root.isEmpty())
    {
      auto& ctx = score::IDocument::documentContext(proc);
      proc.m_root = score::locateFilePath(proc.m_root, ctx);
      // Saved as a path alone: the file is the script.
      if(p.execution.isEmpty())
        p = JS::ProcessModel::readProgramFromFile(proc.m_root);
    }
  }

  proc.setState(st);
  (void)proc.setProgram(p);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
