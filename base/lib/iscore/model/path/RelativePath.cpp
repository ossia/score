#include "RelativePath.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace iscore
{
RelativePath::RelativePath(QObject* obj1, QObject* obj2)
{
  auto obj1_path = iscore::IDocument::unsafe_path(obj1).vec();
  auto obj2_path = iscore::IDocument::unsafe_path(obj2).vec();

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
  ISCORE_ASSERT(source);

  // First go up through the hierarchy
  auto n = m_parents;
  while (n > 0)
  {
    n--;

    source = source->parent();
    ISCORE_ASSERT(source);
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

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const iscore::RelativePath& path)
{
  m_stream << path.m_parents;
  readFrom(path.m_remainder);
}

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamWriter::writeTo(iscore::RelativePath& path)
{
  m_stream >> path.m_parents;
  writeTo(path.m_remainder);
}

template<>
ISCORE_LIB_BASE_EXPORT void
JSONObjectReader::read(const iscore::RelativePath& path)
{
  obj["Parents"] = path.m_parents;
  readFrom(path.m_remainder);
}

template<>
ISCORE_LIB_BASE_EXPORT void
JSONObjectWriter::writeTo(iscore::RelativePath& path)
{
  path.m_parents = obj["Parents"].toInt();
  writeTo(path.m_remainder);
}
