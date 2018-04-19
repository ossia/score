// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RelativePath.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

namespace score
{
RelativePath::RelativePath(QObject* obj1, QObject* obj2)
{
  auto obj1_path = score::IDocument::unsafe_path(obj1).vec();
  auto obj2_path = score::IDocument::unsafe_path(obj2).vec();

  // Find the first common root

  std::size_t i = 0;
  auto min = std::min(obj1_path.size(), obj2_path.size());
  for (; i < min; i++)
  {
    if (!(obj1_path[i] == obj2_path[i]))
      break;
  }

  if (i > 0)
  {
    obj2_path.erase(obj2_path.begin(), obj2_path.begin() + i);

    // Save the number of "parent" objects that we have to go to.
    m_parents = (obj1_path.size() - 1) - i + 1;
    m_remainder = ObjectPath(std::move(obj2_path));
  }
}

QObject* RelativePath::find_impl(QObject* source) const
{
  SCORE_ASSERT(source);

  // First go up through the hierarchy
  auto n = m_parents;
  while (n > 0)
  {
    n--;

    source = source->parent();
    SCORE_ASSERT(source);
  }

  // Then down again
  for (const ObjectIdentifier& currentObjIdentifier : m_remainder.vec())
  {
    auto found_children = source->findChildren<IdentifiedObjectAbstract*>(
        currentObjIdentifier.objectName(), Qt::FindDirectChildrenOnly);

    source = findById_weak_safe(found_children, currentObjIdentifier.id());
  }

  return source;
}
}

template <>
SCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const score::RelativePath& path)
{
  m_stream << path.m_parents;
  readFrom(path.m_remainder);
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(score::RelativePath& path)
{
  m_stream >> path.m_parents;
  writeTo(path.m_remainder);
}

template <>
SCORE_LIB_BASE_EXPORT void
JSONObjectReader::read(const score::RelativePath& path)
{
  obj[strings.Parents] = path.m_parents;
  readFrom(path.m_remainder);
}

template <>
SCORE_LIB_BASE_EXPORT void JSONObjectWriter::write(score::RelativePath& path)
{
  path.m_parents = obj[strings.Parents].toInt();
  writeTo(path.m_remainder);
}
