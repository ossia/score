#include <Process/Dataflow/WidgetInlets.hpp>
#include <score/plugins/SerializableHelpers.hpp>
namespace Process
{
Enum::Enum(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
Enum::Enum(JSONObject::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
Enum::Enum(DataStream::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
Enum::Enum(JSONObject::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}

ComboBox::ComboBox(DataStream::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
ComboBox::ComboBox(JSONObject::Deserializer& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
ComboBox::ComboBox(DataStream::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}
ComboBox::ComboBox(JSONObject::Deserializer&& vis, QObject* parent)
    : ControlInlet{vis, parent}
{
  vis.writeTo(*this);
}

}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::FloatSlider>(const Process::FloatSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::FloatSlider>(Process::FloatSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::FloatSlider>(const Process::FloatSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::FloatSlider>(Process::FloatSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::LogFloatSlider>(
    const Process::LogFloatSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::LogFloatSlider>(Process::LogFloatSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::LogFloatSlider>(
    const Process::LogFloatSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::LogFloatSlider>(Process::LogFloatSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::IntSlider>(const Process::IntSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::IntSlider>(Process::IntSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::IntSlider>(const Process::IntSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::IntSlider>(Process::IntSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::IntSpinBox>(const Process::IntSpinBox& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::IntSpinBox>(Process::IntSpinBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::IntSpinBox>(const Process::IntSpinBox& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::IntSpinBox>(Process::IntSpinBox& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Toggle>(const Process::Toggle& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Toggle>(Process::Toggle& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Toggle>(const Process::Toggle& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Toggle>(Process::Toggle& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ChooserToggle>(const Process::ChooserToggle& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ChooserToggle>(Process::ChooserToggle& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ChooserToggle>(const Process::ChooserToggle& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ChooserToggle>(Process::ChooserToggle& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::LineEdit>(const Process::LineEdit& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::LineEdit>(Process::LineEdit& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::LineEdit>(const Process::LineEdit& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::LineEdit>(Process::LineEdit& p)
{
}


template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Enum>(const Process::Enum& p)
{
  read((const Process::ControlInlet&) p);
  m_stream << p.values << p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Enum>(Process::Enum& p)
{
  m_stream >> p.values >> p.pixmaps;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Enum>(const Process::Enum& p)
{
  read((const Process::ControlInlet&) p);
  obj["Values"] = toJsonValueArray(p.values);
  obj["Pixmaps"] = toJsonValueArray(p.pixmaps);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Enum>(Process::Enum& p)
{
  p.values = fromJsonValueArray<decltype(p.values)>(obj["Values"].toArray());
  p.pixmaps = fromJsonValueArray<decltype(p.pixmaps)>(obj["Pixmaps"].toArray());
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::TimeSignatureChooser>(
    const Process::TimeSignatureChooser& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::TimeSignatureChooser>(
    Process::TimeSignatureChooser& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::TimeSignatureChooser>(
    const Process::TimeSignatureChooser& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::TimeSignatureChooser>(
    Process::TimeSignatureChooser& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::ComboBox>(const Process::ComboBox& p)
{
  read((const Process::ControlInlet&) p);
  m_stream << p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ComboBox>(Process::ComboBox& p)
{
  m_stream >> p.alternatives;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ComboBox>(const Process::ComboBox& p)
{
  read((const Process::ControlInlet&) p);
  obj["Values"] = toJsonValueArray(p.alternatives);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ComboBox>(Process::ComboBox& p)
{
  p.alternatives = fromJsonValueArray<decltype(p.alternatives)>(obj["Values"].toArray());
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Button>(const Process::Button& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Button>(Process::Button& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Button>(const Process::Button& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Button>(Process::Button& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::HSVSlider>(const Process::HSVSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::HSVSlider>(Process::HSVSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::HSVSlider>(const Process::HSVSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::HSVSlider>(Process::HSVSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::XYSlider>(const Process::XYSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::XYSlider>(Process::XYSlider& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::XYSlider>(const Process::XYSlider& p)
{
  read((const Process::ControlInlet&) p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::XYSlider>(Process::XYSlider& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<QString>(const QString& p)
{
  m_stream << p;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<QString>(QString& p)
{
  m_stream >> p;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONValueReader::read<QString>(const QString& p)
{
  val = p;
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONValueWriter::write<QString>(QString& p)
{
  p = val.toString();
}
