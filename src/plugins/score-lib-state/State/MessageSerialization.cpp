// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Message.hpp"
#include "ValueConversion.hpp"
#include "ValueSerialization.hpp"

#include <State/Address.hpp>
#include <State/Value.hpp>

#include <score/serialization/DataStreamVisitor.hpp>

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::Message& mess)
{
  readFrom(mess.address);
  readFrom(mess.value);
  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::Message& mess)
{
  writeTo(mess.address);
  writeTo(mess.value);

  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::Message& mess)
{
  stream.StartObject();
  obj[strings.Address] = mess.address;
  obj[strings.Value] = mess.value;
  stream.EndObject();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::Message& mess)
{
  mess.address <<= obj[strings.Address];
  mess.value <<= obj[strings.Value];
}
