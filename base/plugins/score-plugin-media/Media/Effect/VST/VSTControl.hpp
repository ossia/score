#pragma once
#include <Media/Effect/VST/VSTEffectModel.hpp>

namespace Media::VST
{

class VSTControlInlet final : public Process::Inlet
{
    Q_OBJECT
    SCORE_SERIALIZE_FRIENDS
  public:
      MODEL_METADATA_IMPL(VSTControlInlet)
    using Process::Inlet::Inlet;

    VSTControlInlet(DataStream::Deserializer& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }
    VSTControlInlet(JSONObject::Deserializer& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }
    VSTControlInlet(DataStream::Deserializer&& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }
    VSTControlInlet(JSONObject::Deserializer&& vis, QObject* parent): Inlet{vis, parent}
    {
      vis.writeTo(*this);
    }

    int fxNum{};


    float value() const { return m_value; }
    void setValue(float v)
    {
      if(v != m_value)
      {
        m_value = v;
        emit valueChanged(v);
      }
    }
  Q_SIGNALS:
    void valueChanged(float);

  private:
    float m_value{};
};


}
