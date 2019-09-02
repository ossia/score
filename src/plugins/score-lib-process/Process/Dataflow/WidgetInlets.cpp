#include <Process/Dataflow/WidgetInlets.hpp>
#include <score/plugins/SerializableHelpers.hpp>

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::FloatSlider>(const Process::FloatSlider& p)
{
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
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::Enum>(Process::Enum& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::Enum>(const Process::Enum& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Enum>(Process::Enum& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::TimeSignatureChooser>(
    const Process::TimeSignatureChooser& p)
{
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
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write<Process::ComboBox>(Process::ComboBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read<Process::ComboBox>(const Process::ComboBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::ComboBox>(Process::ComboBox& p)
{
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::Button>(const Process::Button& p)
{
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
}
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write<Process::Button>(Process::Button& p)
{
}
