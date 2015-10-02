#pragma once
#include <Curve/StateMachine/CurvePoint.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class CurveSegmentModel;
struct CurveSegmentData;
/* TODO it would maybe faster to have them on the heap and use QPointer for
 * caching ...
class SegmentId
{
    public:
        using value_type = boost::optional<int32_t>;
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
struct CurveSegmentData
{
        Id<CurveSegmentModel> id;

        CurvePoint start, end;
        Id<CurveSegmentModel> previous, following;

        QString type;
        QVariant specificSegmentData;

        double x() const {
            return start.x();
        }
        bool operator<(const CurveSegmentData& other) const
        { return x() < other.x(); }
        bool operator<=(const CurveSegmentData& other) const
        { return x() <= other.x(); }
};


Q_DECLARE_METATYPE(CurveSegmentData)


#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

// We don't want crashes on invalid ids search
class CurveDataHash
{
    public:
        std::size_t operator()(const Id<CurveSegmentModel>& id) const
        {
            if(!id)
                return -1;

            return *id.val();
        }
};

namespace bmi = boost::multi_index;
using CurveSegmentMap = bmi::multi_index_container<
    CurveSegmentData,
    bmi::indexed_by<
        bmi::hashed_unique<
            bmi::member<
                CurveSegmentData,
                Id<CurveSegmentModel>,
                &CurveSegmentData::id
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
                ISCORE_ASSERT(id.m_ptr->parent() == (*this->map.find(id))->parent());
                return safe_cast<CurveSegmentData&>(*id.m_ptr);
            }
            auto item = this->map.find(id);
            ISCORE_ASSERT(item != this->map.end());

            id.m_ptr = *item;
            return safe_cast<Element&>(**item);
        }
};
*/
using CurveSegmentOrderedMap = bmi::multi_index_container<
CurveSegmentData,
bmi::indexed_by<
bmi::hashed_unique<
bmi::member<
CurveSegmentData,
Id<CurveSegmentModel>,
&CurveSegmentData::id
>, CurveDataHash
>,
bmi::ordered_unique<
bmi::identity<CurveSegmentData>
>
>
>;

enum Segments { Hashed, Ordered };
