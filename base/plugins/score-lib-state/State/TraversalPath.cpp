// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TraversalPath.hpp"

#include <ossia/network/common/path.hpp>

#include <wobjectimpl.h>
W_GADGET_IMPL(State::TraversalPath)
namespace State
{

TraversalPath::TraversalPath() noexcept
    : m_textual{}, m_path{std::make_unique<ossia::traversal::path>()}
{
}

TraversalPath::TraversalPath(const TraversalPath& other) noexcept
    : m_textual{other.m_textual}
    , m_path{std::make_unique<ossia::traversal::path>(*other.m_path)}
{
}

TraversalPath::TraversalPath(TraversalPath&& other) noexcept
    : m_textual{std::move(other.m_textual)}, m_path{std::move(other.m_path)}
{
  other.m_path = std::make_unique<ossia::traversal::path>();
}

TraversalPath& TraversalPath::operator=(const TraversalPath& other) noexcept
{
  m_textual = other.m_textual;
  *m_path = *other.m_path;
  return *this;
}

TraversalPath& TraversalPath::operator=(TraversalPath&& other) noexcept
{
  m_textual = std::move(other.m_textual);
  *m_path = std::move(*other.m_path);
  return *this;
}

TraversalPath::~TraversalPath()
{
}

TraversalPath::TraversalPath(
    const QString& s, const ossia::traversal::path& p) noexcept
    : m_textual{s}, m_path{std::make_unique<ossia::traversal::path>(p)}
{
}

TraversalPath::TraversalPath(QString&& s, ossia::traversal::path&& p) noexcept
    : m_textual{std::move(s)}
    , m_path{std::make_unique<ossia::traversal::path>(std::move(p))}
{
}

optional<TraversalPath> TraversalPath::make_path(QString str)
{
  if (auto p = ossia::traversal::make_path(str.toStdString()))
  {
    return TraversalPath{std::move(str), *std::move(p)};
  }
  else
  {
    return {};
  }
}

bool TraversalPath::operator==(const TraversalPath& other) const noexcept
{
  return m_textual == other.m_textual;
}

bool TraversalPath::operator!=(const TraversalPath& other) const noexcept
{
  return m_textual != other.m_textual;
}

const ossia::traversal::path& TraversalPath::get() const noexcept
{
  return *m_path;
}

ossia::traversal::path& TraversalPath::get() noexcept
{
  return *m_path;
}

TraversalPath::operator const ossia::traversal::path&() const noexcept
{
  return *m_path;
}

TraversalPath::operator ossia::traversal::path&() noexcept
{
  return *m_path;
}
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::TraversalPath& var)
{
  m_stream << var.m_textual;
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::TraversalPath& var)
{
  m_stream >> var.m_textual;
  *var.m_path = *ossia::traversal::make_path(var.m_textual.toStdString());
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const State::TraversalPath& var)
{
  val = var.m_textual;
}

template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(State::TraversalPath& var)
{
  var.m_textual = val.toString();
  *var.m_path = *ossia::traversal::make_path(var.m_textual.toStdString());
}
