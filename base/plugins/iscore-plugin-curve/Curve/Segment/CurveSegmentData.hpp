#pragma once
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <Curve/Palette/CurvePoint.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

namespace Curve
{
class SegmentFactory;
class SegmentModel;
struct SegmentData;
/* TODO it would maybe faster to have them on the heap and use QPointer for
 * caching ...
class SegmentId
{
    public:
        using value_type = optional<int32_t>;
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
      OptionalId<SegmentModel>
          prev,
      OptionalId<SegmentModel>
          foll,
      UuidKey<Curve::SegmentFactory>
          t,
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

  double x() const
  {
    return start.x();
  }
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
    id = Id<SegmentModel>{iscore::random_id_generator::getRandomId()};
  } while (ids.find(id) != end);

  return id;
}

inline Id<SegmentModel> getSegmentId(const std::vector<SegmentData>& ids)
{
  Id<SegmentModel> id{};

  auto end = ids.end();
  do
  {
    id = Id<SegmentModel>{iscore::random_id_generator::getRandomId()};
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
    id = Id<SegmentModel>{iscore::random_id_generator::getRandomId()};
  } while (ossia::find_if(ids, [&](const auto& other) { return other == id; })
           != end);

  return id;
}

// We don't want crashes on invalid ids search
class CurveDataHash
{
public:
  std::size_t operator()(const Id<SegmentModel>& id) const
  {
    return id.val();
  }
};

namespace bmi = boost::multi_index;
using CurveSegmentMap = bmi::multi_index_container<
    SegmentData,
    bmi::indexed_by<
        bmi::hashed_unique<
            bmi::member<
                SegmentData,
                Id<SegmentModel>,
                &SegmentData::id
            >, CurveDataHash

        >
    >
>;
/*
class CurveSegmentCachingMap : private CurveSegmentMap
{
        using CurveSegmentMap::CurveSegmentMap;

        CurveSegmentData& at(const Id<CurveSegmentModel>& id) const
        {
            if(id.m_ptr)
            {
                ISCORE_ASSERT(id.m_ptr->parent() ==
(*this->map.find(id))->parent());
                return safe_cast<CurveSegmentData&>(*id.m_ptr);
            }
            auto item = this->map.find(id);
            ISCORE_ASSERT(item != this->map.end());

            id.m_ptr = *item;
            return safe_cast<Element&>(**item);
        }
};
*/
using CurveSegmentOrderedMap = bmi::
    multi_index_container<SegmentData, bmi::indexed_by<bmi::hashed_unique<bmi::member<SegmentData, Id<SegmentModel>, &SegmentData::id>, CurveDataHash>, bmi::ordered_unique<bmi::identity<SegmentData>>>>;

enum Segments
{
  Hashed,
  Ordered
};
}

Q_DECLARE_METATYPE(Curve::SegmentData)

#define CURVE_SEGMENT_FACTORY_METADATA(Export, Model, Uuid) \
  template <>                                               \
  struct Export Metadata<ConcreteKey_k, Model>       \
  {                                                         \
    static const auto& get()                                \
    {                                                       \
      static const UuidKey<Curve::SegmentFactory> k{Uuid};  \
      return k;                                             \
    }                                                       \
  };

#define CURVE_SEGMENT_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
  OBJECTKEY_METADATA(Export, Model, ObjectKey)                             \
  CURVE_SEGMENT_FACTORY_METADATA(Export, Model, Uuid)                      \
  template <>                                                              \
  struct Export Metadata<PrettyName_k, Model>                              \
  {                                                                        \
    static auto get()                                                      \
    {                                                                      \
      return QObject::tr(PrettyName);                                      \
    }                                                                      \
  };
