#pragma once
#include <Curve/StateMachine/CurvePoint.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class CurveSegmentModel;

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
