#include "SharedInputSettings.hpp"
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

template <>
void DataStreamReader::read(const Gfx::SharedInputSettings& n)
{
  m_stream << n.path;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::SharedInputSettings& n)
{
  m_stream >> n.path;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::SharedInputSettings& n)
{
  obj["Path"] = n.path;
}

template <>
void JSONWriter::write(Gfx::SharedInputSettings& n)
{
  n.path = obj["Path"].toString();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::SharedInputSettings);
