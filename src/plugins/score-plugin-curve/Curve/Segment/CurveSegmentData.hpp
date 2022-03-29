#pragma once
#include <Curve/Palette/CurvePoint.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/serialization/DataStreamFwd.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QVariant>

namespace Curve
{
class SegmentFactory;
class SegmentModel;
class Category_k;
struct SegmentData;
/* TODO it would maybe faster to have them on the heap and use QPointer for
 * caching ...
class SegmentId
{
    public:
        using value_type = std::optional<int32_t>;
        explicit SegmentId() = default;
        explicit SegmentId(value_type val) : m_id {val} { }
        SegmentId(const SegmentId&) = delete;
        SegmentId(SegmentId&& ) = delete;
        SegmentId& operator=(const SegmentId&) = delete;
        SegmentId& operator=(SegmentId&& ) = delete;

        friend bool operator== (const id_base_t& lhs, const id_base_t& rhs)
        { return lhs.m_id == rhs.m_id; }

        friend bool operator!= (const id_base_t& lhs, const id_base_t& rhs)
        { return lhs.m_id != rhs.m_id; }

        friend bool operator< (const id_base_t& lhs, const id_base_t& rhs)
        { return *lhs.val() < *rhs.val(); }

        explicit operator bool() const
        { return bool(m_id); }

        explicit operator value_type() const
        { return m_id; }

        const value_type& val() const
        { return m_id; }

        void setVal(value_type&& val)
        { m_id = val; }

        void unset()
        { m_id = value_type(); }

    private:
        mutable CurveSegmentData* m_ptr{};
        value_type m_id {};
};*/

// An object wrapper useful for saving / loading
struct SegmentData
{
  SegmentData() = default;
  SegmentData(const SegmentData&) = default;
  SegmentData(SegmentData&&) = default;
  SegmentData& operator=(const SegmentData&) = default;
  SegmentData& operator=(SegmentData&&) = default;

  SegmentData(
      Id<SegmentModel> i,
      Curve::Point s,
      Curve::Point e,
      OptionalId<SegmentModel> prev,
      OptionalId<SegmentModel> foll,
      const UuidKey<Curve::SegmentFactory>& t,
      QVariant data)
      : id(std::move(i))
      , start(s)
      , end(e)
      , previous(std::move(prev))
      , following(std::move(foll))
      , type(t)
      , specificSegmentData(std::move(data))
  {
  }

  Id<SegmentModel> id;

  Curve::Point start, end;
  OptionalId<SegmentModel> previous, following;

  UuidKey<Curve::SegmentFactory> type;
  QVariant specificSegmentData;

  double x() const { return start.x(); }
};

inline bool operator<(const SegmentData& lhs, const SegmentData& rhs)
{
  return lhs.x() < rhs.x();
}

inline bool operator<=(const SegmentData& lhs, const SegmentData& rhs)
{
  return lhs.x() <= rhs.x();
}

// REFACTORME with SettableIdentifierGeneration...
template <typename Container>
Id<SegmentModel> getSegmentId(const Container& ids)
{
  Id<SegmentModel> id{};
  auto end = ids.end();
  do
  {
    id = Id<SegmentModel>{score::random_id_generator::getRandomId()};
  } while (ids.find(id) != end);

  return id;
}

inline Id<SegmentModel> getSegmentId(const std::vector<SegmentData>& ids)
{
  Id<SegmentModel> id{};

  auto end = ids.end();
  do
  {
    id = Id<SegmentModel>{score::random_id_generator::getRandomId()};
  } while (
      ossia::find_if(ids, [&](const auto& other) { return other.id == id; })
      != end);

  return id;
}

inline Id<SegmentModel> getSegmentId(const std::vector<Id<SegmentModel>>& ids)
{
  Id<SegmentModel> id{};

  auto end = ids.end();
  do
  {
    id = Id<SegmentModel>{score::random_id_generator::getRandomId()};
  } while (ossia::find_if(ids, [&](const auto& other) { return other == id; })
           != end);

  return id;
}

// We don't want crashes on invalid ids search
class CurveDataHash
{
public:
  std::size_t operator()(const Id<SegmentModel>& id) const { return id.val(); }
};

enum Segments
{
  Hashed
};
}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Curve::SegmentData)
Q_DECLARE_METATYPE(Curve::SegmentData)
W_REGISTER_ARGTYPE(Curve::SegmentData)

#define CURVE_SEGMENT_FACTORY_METADATA(Export, Model, Uuid) \
  template <>                                               \
  struct Export Metadata<ConcreteKey_k, Model>              \
  {                                                         \
    static const auto& get()                                \
    {                                                       \
      static const UuidKey<Curve::SegmentFactory> k{Uuid};  \
      return k;                                             \
    }                                                       \
  };

#define CURVE_SEGMENT_METADATA(                           \
    Export, Model, Uuid, ObjectKey, PrettyName, Category) \
  OBJECTKEY_METADATA(Export, Model, ObjectKey)            \
  CURVE_SEGMENT_FACTORY_METADATA(Export, Model, Uuid)     \
  template <>                                             \
  struct Export Metadata<PrettyName_k, Model>             \
  {                                                       \
    static auto get() { return QObject::tr(PrettyName); } \
  };                                                      \
  template <>                                             \
  struct Export Metadata<Curve::Category_k, Model>        \
  {                                                       \
    static auto get() { return QObject::tr(Category); }   \
  };
