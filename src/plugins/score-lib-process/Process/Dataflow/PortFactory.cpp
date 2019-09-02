#include <Process/Dataflow/PortFactory.hpp>
#include <score/plugins/SerializableHelpers.hpp>

namespace Process
{

void writeInlets(
    DataStreamWriter& wr,
    const Process::PortFactoryList& pl,
    Process::Inlets& ports,
    QObject* parent)
{
  int32_t count;
  wr.m_stream >> count;

  for (; count-- > 0;)
  {
    auto proc = deserialize_interface(pl, wr, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Inlet*>(proc));
    }
    else
    {
      SCORE_ABORT;
    }
  }
}
void writeOutlets(
    DataStreamWriter& wr,
    const Process::PortFactoryList& pl,
    Process::Outlets& ports,
    QObject* parent)
{
  int32_t count;
  wr.m_stream >> count;

  for (; count-- > 0;)
  {
    auto proc = deserialize_interface(pl, wr, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Outlet*>(proc));
    }
    else
    {
      SCORE_ABORT;
    }
  }
}
void writeInlets(
    const QJsonArray& wr,
    const Process::PortFactoryList& pl,
    Process::Inlets& ports,
    QObject* parent)
{
  for (const auto& json_vref : wr)
  {
    const auto& obj = json_vref.toObject();
    auto proc = deserialize_interface(
        pl, JSONObject::Deserializer{obj}, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Inlet*>(proc));
    }
    else
    {
      if(!obj.contains("uuid")) {
        // Old format
        auto p = new Process::Inlet(Id<Process::Inlet>(obj["id"].toInt()), parent);
        p->setAddress(fromJsonObject<State::AddressAccessor>(obj["Address"].toObject()));
        p->setCustomData(obj["Custom"].toString());
        auto cables = obj["Cables"].toArray();
        for(auto c : cables) {
          // TODO
        }

        auto t = obj["type"].toInt();
        switch(t) {
          case 0: // Message
          {
            p->type = Process::PortType::Message;
            ports.push_back(p);
            return;
          }
          case 1: // Audio
          {
            p->type = Process::PortType::Audio;
            ports.push_back(p);
            return;
          }
          case 2: // MIDI
          {
            p->type = Process::PortType::Midi;
            ports.push_back(p);
            return;
          }
           default:
            break;
        }
      }

      // Serialization bug
      SCORE_ABORT;
    }
  }
}
void writeOutlets(
    const QJsonArray& wr,
    const Process::PortFactoryList& pl,
    Process::Outlets& ports,
    QObject* parent)
{
  for (const auto& json_vref : wr)
  {
    const auto& obj = json_vref.toObject();
    auto proc = deserialize_interface(
        pl, JSONObject::Deserializer{obj}, parent);
    if (proc)
    {
      ports.push_back(safe_cast<Process::Outlet*>(proc));
    }
    else
    {
      if(!obj.contains("uuid")) {
        // Old format
        auto p = new Process::Outlet(Id<Process::Inlet>(obj["id"].toInt()), parent);
        p->setAddress(fromJsonObject<State::AddressAccessor>(obj["Address"].toObject()));
        p->setCustomData(obj["Custom"].toString());
        p->setPropagate(obj["Propagate"].toBool());
        auto cables = obj["Cables"].toArray();
        for(auto c : cables) {
          // TODO
        }

        auto t = obj["type"].toInt();
        switch(t) {
          case 0: // Message
          {
            p->type = Process::PortType::Message;
            ports.push_back(p);
            return;
          }
          case 1: // Audio
          {
            p->type = Process::PortType::Audio;
            ports.push_back(p);
            return;
          }
          case 2: // MIDI
          {
            p->type = Process::PortType::Midi;
            ports.push_back(p);
            return;
          }
           default:
            break;
        }
      }

      // Serialization bug
      SCORE_ABORT;
    }
  }
}
void readPorts(
    DataStreamReader& wr,
    const Process::Inlets& ins,
    const Process::Outlets& outs)
{
  wr.m_stream << (int32_t)ins.size();
  for (auto v : ins)
    wr.readFrom(*v);
  wr.m_stream << (int32_t)outs.size();
  for (auto v : outs)
    wr.readFrom(*v);
}
void writePorts(
    DataStreamWriter& wr,
    const Process::PortFactoryList& pl,
    Process::Inlets& ins,
    Process::Outlets& outs,
    QObject* parent)
{
  writeInlets(wr, pl, ins, parent);
  writeOutlets(wr, pl, outs, parent);
}
void readPorts(
    QJsonObject& obj,
    const Process::Inlets& ins,
    const Process::Outlets& outs)
{
  obj["Inlets"] = toJsonArray(ins);
  obj["Outlets"] = toJsonArray(outs);
}
void writePorts(
    const QJsonObject& obj,
    const Process::PortFactoryList& pl,
    Process::Inlets& ins,
    Process::Outlets& outs,
    QObject* parent)
{
  writeInlets(obj["Inlets"].toArray(), pl, ins, parent);
  writeOutlets(obj["Outlets"].toArray(), pl, outs, parent);
}
}
