#pragma once
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

/**
 * @brief The IdentifiedObject class
 *
 * An object with an unique identifier. This identifier
 * is used to find objects in a path.
 *
 * A class should only have a single child of the same type with a given
 * identifier
 * since QObject::findChild is used.
 *
 */
template <typename model>
class IdentifiedObject : public IdentifiedObjectAbstract
{
public:
  static const constexpr bool identified_object_tag = true;
  using model_type = model;
  using id_type = Id<model>;

  IdentifiedObject(id_type id, const QString& name, QObject* parent) noexcept
      : IdentifiedObjectAbstract{name, parent}, m_id{std::move(id)}
  {
    m_id.m_ptr = this;
  }

  template <typename Visitor>
  IdentifiedObject(Visitor&& v, QObject* parent) noexcept
      : IdentifiedObjectAbstract{parent}
  {
    using vis_type = typename std::remove_reference_t<Visitor>::type;
    TSerializer<vis_type, IdentifiedObject<model>>::writeTo(v, *this);
    m_id.m_ptr = this;
  }

  ~IdentifiedObject() override = default;

  const id_type& id() const noexcept
  {
    return m_id;
  }

  int32_t id_val() const noexcept final override
  {
    return m_id.val();
  }

  void setId(const id_type& id) noexcept
  {
    m_id = id;
    m_path_cache.unsafePath().vec().clear();
    m_id.m_ptr = this;
  }

  void setId(id_type&& id) noexcept
  {
    m_id = std::move(id);
    m_path_cache.unsafePath().vec().clear();
    m_id.m_ptr = this;
  }

  void resetCache() final override
  {
    m_path_cache.unsafePath().vec().clear();
  }

  mutable Path<model> m_path_cache;
  // TODO see
  // http://stackoverflow.com/questions/32987869/befriending-of-function-template-with-enable-if
  // to put in private
private:
  id_type m_id{};
};

template <typename model>
std::size_t hash_value(const Id<model>& id) noexcept
{
  return id.val();
}

template <typename T, typename U>
bool operator==(const T* obj, const Id<U>& id) noexcept
{
  return obj->id() == id;
}

template <typename T, typename U, typename = decltype(std::declval<T>().id())>
bool operator==(const T& obj, const Id<U>& id) noexcept
{
  return obj.id() == id;
}

namespace score
{
namespace IDocument
{

/**
 * @brief path Typesafe path of an object in a document.
 * @param obj The object of which path is to be created.
 * @return A path to the object if it is in a document
 *
 * This function will abort the software if given an object
 * not in a document hierarchy in argument.
 */
template <typename T>
Path<T> path(const IdentifiedObject<T>& obj)
{
  static_assert(!std::is_pointer<T>::value, "Don't pass a pointer to path");
  if (obj.m_path_cache.valid())
    return obj.m_path_cache;

  obj.m_path_cache = Path<T>{
      score::IDocument::unsafe_path(safe_cast<const QObject&>(obj)), {}};
  return obj.m_path_cache;
}
}
}
