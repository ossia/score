#include "LeapmotionSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::LeapmotionSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::LeapmotionSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::LeapmotionSpecificSettings& n)
{
}

template <>
void JSONWriter::write(Protocols::LeapmotionSpecificSettings& n)
{
}
