// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DataStreamVisitor.hpp"
#include "JSONValueVisitor.hpp"

#include <array>

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const std::array<float, 2>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream << obj[i];
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(std::array<float, 2>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream >> obj[i];
  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const std::array<float, 3>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream << obj[i];
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(std::array<float, 3>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream >> obj[i];
  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const std::array<float, 4>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream << obj[i];
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(std::array<float, 4>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream >> obj[i];
  checkDelimiter();
}
