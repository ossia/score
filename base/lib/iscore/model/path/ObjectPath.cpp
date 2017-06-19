#include <iscore/tools/std/Optional.hpp>

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QObject>
#include <QStringBuilder>
#include <iscore/model/path/ObjectPath.hpp>
#include <iscore/model/path/RelativePath.hpp>
#include <iterator>
#include <qnamespace.h>
#include <stdexcept>
#include <sys/types.h>
#include <typeinfo>

#include <boost/range.hpp>
#include <iscore/model/IdentifiedObjectAbstract.hpp>

#include <ossia/detail/algorithms.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/model/path/ObjectIdentifier.hpp>

namespace boost
{
ISCORE_LIB_BASE_EXPORT std::size_t hash<ObjectIdentifier>::operator()(const ObjectIdentifier& path) const
{
  std::size_t seed = 0;
  boost::hash_combine(seed, path.objectName());
  boost::hash_combine(seed, path.id());
  return seed;
}
ISCORE_LIB_BASE_EXPORT std::size_t hash<ObjectPath>::operator()(const ObjectPath& path) const
{
  return boost::hash_range(path.vec().cbegin(), path.vec().cend());
}
}

ObjectPath ObjectPath::pathBetweenObjects(
    const QObject* const parent_obj, const QObject* target_object)
{
  std::vector<ObjectIdentifier> v;

  auto current_obj = target_object;
  auto add_parent_to_vector = [&v](const QObject* ptr) {
    if (auto id_obj = qobject_cast<const IdentifiedObjectAbstract*>(ptr))
      v.push_back({id_obj->objectName(), id_obj->id_val()});
    else
      v.push_back({ptr->objectName(), {}});
  };
#if defined(ISCORE_DEBUG)
  QString debug_objectnames;
#endif
  // Recursively go through the object and all the parents
  while (current_obj != parent_obj)
  {
#if defined(ISCORE_DEBUG)
    debug_objectnames += current_obj->objectName() + "->";
#endif
    if (current_obj->objectName().isEmpty())
    {
#if defined(ISCORE_DEBUG)
      qDebug() << "Names: " << debug_objectnames;
#endif
      ISCORE_BREAKPOINT;
      throw std::runtime_error(
          "ObjectPath::pathFromObject : an object in the hierarchy does not "
          "have a name.");
    }

    add_parent_to_vector(current_obj);

    current_obj = current_obj->parent();

    if (!current_obj)
    {
      ISCORE_BREAKPOINT;
      throw std::runtime_error(
          "ObjectPath::pathFromObject : Could not find path to parent object");
    }
  }

  // Add the last parent (the one specified with parent_name)
  add_parent_to_vector(current_obj);

  // Search goes from top to bottom (of the parent hierarchy) instead
  std::reverse(std::begin(v), std::end(v));

  ObjectPath path{std::move(v)};
  path.m_cache = const_cast<QObject*>(target_object);

  return path;
}

QString ObjectPath::toString() const
{
  QString s;

  for (auto& obj : m_objectIdentifiers)
  {
    s += obj.objectName() % "." % QString::number(obj.id()) % "/";
  }

  return s;
}

QObject* ObjectPath::find_impl(const iscore::DocumentContext& ctx) const
{
  using namespace iscore;
  QObject* obj = &ctx.document.model();

  auto children = boost::make_iterator_range(
      m_objectIdentifiers.begin() + 1, m_objectIdentifiers.end());
  for (const auto& currentObjIdentifier : children)
  {
    auto found_children = obj->findChildren<IdentifiedObjectAbstract*>(
        currentObjIdentifier.objectName(), Qt::FindDirectChildrenOnly);

    obj = findById_weak_safe(found_children, currentObjIdentifier.id());
  }

  return obj;
}

QObject* ObjectPath::find_impl_unsafe(const iscore::DocumentContext& ctx) const
{
  using namespace iscore;
  QObject* obj = &ctx.document.model();

  auto children = boost::make_iterator_range(
      std::begin(m_objectIdentifiers) + 1, std::end(m_objectIdentifiers));
  for (const auto& currentObjIdentifier : children)
  {
    auto found_children = obj->findChildren<IdentifiedObjectAbstract*>(
        currentObjIdentifier.objectName(), Qt::FindDirectChildrenOnly);

    obj = findById_weak_unsafe(found_children, currentObjIdentifier.id());

    if (!obj)
      return nullptr;
  }

  return obj;
}
