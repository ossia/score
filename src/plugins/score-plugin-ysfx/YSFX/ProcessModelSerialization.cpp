// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QString>

static
QByteArray readYSFXState(ysfx_t& fx)
{
  QByteArray dat;
  {
    auto state = ysfx_save_state(&fx);
    int64_t sz = state->data_size;
    int64_t sliders = state->slider_count;

    {
      QDataStream stream{&dat, QIODevice::WriteOnly};
      stream << sz << sliders;
      stream << QByteArray((const char*)state->data, sz);
      for(int64_t i = 0; i < sliders; i++) {
        stream << state->sliders[i].index;
        stream << state->sliders[i].value;
      }
    }

    ysfx_state_free(state);
  }

  return dat;
}

static
void loadYSFXState(ysfx_t& fx, const QByteArray& state)
{
  ysfx_state_t s;
  int64_t sz{};
  int64_t sliders{};
  QByteArray dat{};
  std::vector<ysfx_state_slider_s> sliders_data;

  QDataStream stream{state};
  {
    stream >> sz >> sliders >> dat;
    sliders_data.resize(sliders);

    for(int64_t i = 0; i < sliders; i++) {
      stream >> sliders_data[i].index;
      stream >> sliders_data[i].value;
    }
  }

  s.data = (uint8_t*)dat.data();
  s.data_size = sz;
  s.slider_count = sliders;
  s.sliders = sliders_data.data();

  ysfx_load_state(&fx, &s);
}

template <>
void DataStreamReader::read(const YSFX::ProcessModel& proc)
{
  m_stream << proc.m_script << readYSFXState(*proc.fx.get());

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(YSFX::ProcessModel& proc)
{
  QString str;
  QByteArray dat;
  m_stream >> str >> dat;
  proc.setScript(str);

  loadYSFXState(*proc.fx.get(), dat);

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const YSFX::ProcessModel& proc)
{
  obj["Script"] = proc.script();
  obj["Chunk"] = readYSFXState(*proc.fx.get()).toBase64();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(YSFX::ProcessModel& proc)
{
  proc.setScript(obj["Script"].toString());
  auto dat = QByteArray::fromBase64(obj["Chunk"].toByteArray());
  loadYSFXState(*proc.fx.get(), dat);

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
