// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/application/ApplicationContext.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash.hpp>
#include <ossia/detail/murmur3.hpp>

#include <QObject>
#include <QStringBuilder>
#include <qnamespace.h>

#include <sys/types.h>

#include <stdexcept>
#include <typeinfo>

namespace std
{

SCORE_LIB_BASE_EXPORT std::size_t hash<ObjectIdentifier>::
operator()(const ObjectIdentifier& path) const
{
  uint32_t seed = 0;
  ossia::murmur::murmur3_x86_32(
      path.objectName().data(), path.objectName().size() * 2, seed, seed);
  ossia::hash_combine(seed, path.id());
  return seed;
}
SCORE_LIB_BASE_EXPORT std::size_t hash<ObjectPath>::
operator()(const ObjectPath& path) const
{
  std::size_t seed = 0;
  for (const auto& v : path.vec())
  {
    ossia::hash_combine(seed, hash<ObjectIdentifier>{}(v));
  }
  return seed;
}
}


template <typename Container>
typename Container::value_type
findById_weak_safe(const Container& c, int32_t id)
{
  auto it = std::find_if(
      std::begin(c), std::end(c), [&id](typename Container::value_type model) {
        return model->id_val() == id;
      });

  if (it != std::end(c))
  {
    return *it;
  }

  SCORE_BREAKPOINT;
  throw std::runtime_error(
      QString("findById : id %1 not found in vector of %2")
          .arg(id)
          .arg(typeid(c).name())
          .toUtf8()
          .constData());
}

template <typename Container>
typename Container::value_type
findById_weak_unsafe(const Container& c, int32_t id) noexcept
{
  auto it = std::find_if(
      std::begin(c), std::end(c), [&id](typename Container::value_type model) {
        return model->id_val() == id;
      });

  if (it != std::end(c))
  {
    return *it;
  }

  return nullptr;
}



ObjectPath ObjectPath::pathBetweenObjects(
    const QObject* const parent_obj,
    const QObject* target_object)
{
  std::vector<ObjectIdentifier> v;

  auto current_obj = target_object;
  auto add_parent_to_vector = [&v](const QObject* ptr) {
    if (auto id_obj = qobject_cast<const IdentifiedObjectAbstract*>(ptr))
      v.push_back({id_obj->objectName(), id_obj->id_val()});
    else
      v.push_back({ptr->objectName(), {}});
  };
#if defined(SCORE_DEBUG)
  QString debug_objectnames;
#endif
  // Recursively go through the object and all the parents
  while (current_obj != parent_obj)
  {
#if defined(SCORE_DEBUG)
    debug_objectnames += current_obj->objectName() + "->";
#endif
    if (current_obj->objectName().isEmpty())
    {
#if defined(SCORE_DEBUG)
      qDebug() << "Names: " << debug_objectnames;
#endif
      SCORE_BREAKPOINT;
      throw std::runtime_error(
          "ObjectPath::pathFromObject : an object in the hierarchy does not "
          "have a name.");
    }

    add_parent_to_vector(current_obj);

    current_obj = current_obj->parent();

    if (!current_obj)
    {
      SCORE_BREAKPOINT;
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

QString ObjectPath::toString() const noexcept
{
  QString s;

  for (auto& obj : m_objectIdentifiers)
  {
    s += obj.objectName() % "." % QString::number(obj.id()) % "/";
  }

  return s;
}

QObject* ObjectPath::find_impl(const score::DocumentContext& ctx) const
{
  using namespace score;
  QObject* obj = &ctx.document.model();
  SCORE_ASSERT(obj);

  for (const auto& currentObjIdentifier : m_objectIdentifiers)
  {
    const QObjectList &children = obj->children();
    QObject* found = nullptr;

    for (int i = 0; i < children.size(); ++i)
    {
      obj = children.at(i);
      SCORE_ASSERT(obj);
      if (obj->objectName() == currentObjIdentifier.objectName())
      {
        auto itf = safe_cast<IdentifiedObjectAbstract*>(obj);
        if(itf->id_val() == currentObjIdentifier.id())
        {
          found = itf;
          break;
        }
      }
    }

    if(found)
    {
      obj = found;
      SCORE_ASSERT(obj);
    }
    else
    {
      SCORE_BREAKPOINT;
      throw std::runtime_error(
          QString("findById : id %1 not found")
              .arg(currentObjIdentifier.id())
              .toUtf8()
              .constData());
    }
  }

  return obj;
}

QObject* ObjectPath::find_impl_unsafe(const score::DocumentContext& ctx) const
    noexcept
{
  using namespace score;
  QObject* obj = &ctx.document.model();
  SCORE_ASSERT(obj);

  for (const auto& currentObjIdentifier : m_objectIdentifiers)
  {
    const QObjectList &children = obj->children();
    QObject* found = nullptr;

    for (int i = 0; i < children.size(); ++i)
    {
      obj = children.at(i);
      if (obj->objectName() == currentObjIdentifier.objectName())
      {
        auto itf = safe_cast<IdentifiedObjectAbstract*>(obj);
        if(itf->id_val() == currentObjIdentifier.id())
        {
          found = itf;
          break;
        }
      }
    }

    if(found)
    {
      obj = found;
    }
    else
    {
      return nullptr;
    }
  }

  return obj;
}

void replacePathPart(
    const ObjectPath& src,
    const ObjectPath& target,
    ObjectPath& toChange)
{
  auto& src_v = src.vec();
  auto& tgt_v = target.vec();
  auto& v = toChange.vec();
#ifdef SCORE_DEBUG
  SCORE_ASSERT(v.size() > src_v.size());
  for (std::size_t i = 0; i < src_v.size(); i++)
  {
    SCORE_ASSERT(v[i] == src_v[i]);
  }
#endif

  // OPTIMIZEME
  v.erase(v.begin(), v.begin() + src_v.size());
  v.insert(v.begin(), tgt_v.begin(), tgt_v.end());
}
