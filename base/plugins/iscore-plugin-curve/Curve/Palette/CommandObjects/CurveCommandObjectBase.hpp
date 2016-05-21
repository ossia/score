#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <QPoint>
#include <QVector>
#include <algorithm>
#include <vector>

#include <Curve/Segment/CurveSegmentData.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_curve_export.h>

namespace iscore {
class CommandStackFacade;
}  // namespace iscore

/*
concept CommandObject
{
    public:
        void instantiate();
        void update();
        void commit();
        void rollback();
};
*/
// CreateSegment
// CreateSegmentBetweenPoints

// RemoveSegment -> easy peasy
// RemovePoint -> which segment do we merge ? At the left or at the right ?
// A point(view) has pointers to one or both of its curve segments.

namespace Curve
{
class UpdateCurve;
class Model;
class Presenter;
class StateBase;
class SegmentModel;

class CommandObjectBase
{
    public:
        CommandObjectBase(Presenter* pres, iscore::CommandStackFacade&);
        virtual ~CommandObjectBase();

        void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }
        void press();


        void handleLocking();

        // Creates and pushes an UpdateCurve command
        // from a vector of segments.
        // They are removed afterwards
        void submit(std::vector<SegmentData>&&);

    protected:

        auto find(
                std::vector<SegmentData>& segments,
                const Id<SegmentModel>& id)
        {
            return std::find_if(
                        segments.begin(),
                        segments.end(),
                        [&] (const auto& seg) { return seg.id == id; });
        }
        auto find(
                const std::vector<SegmentData>& segments,
                const Id<SegmentModel>& id)
        {
            return std::find_if(
                        segments.cbegin(),
                        segments.cend(),
                        [&] (const auto& seg) { return seg.id == id; });
        }

        template<typename Container>
        Id<SegmentModel> getSegmentId(const Container& ids)
        {
            Id<SegmentModel> id {};

            do
            {
                id = Id<SegmentModel>{iscore::random_id_generator::getRandomId()};
            }
            while(ids.find(id) != ids.end());

            return id;
        }

        Id<SegmentModel> getSegmentId(const std::vector<SegmentData>& ids)
        {
            Id<SegmentModel> id {};

            do
            {
                id = Id<SegmentModel>{iscore::random_id_generator::getRandomId()};
            }
            while(std::find_if(ids.begin(),
                               ids.end(),
                               [&] (const auto& other) { return other.id == id; }) != ids.end());

            return id;
        }

        virtual void on_press() = 0;

        QVector<QByteArray> m_oldCurveData;
        QPointF m_originalPress; // Note : there should be only one per curve...

        Presenter* m_presenter{};

        Curve::StateBase* m_state{};

        SingleOngoingCommandDispatcher<UpdateCurve> m_dispatcher;
        Path<Model> m_modelPath;

        std::vector<SegmentData> m_startSegments;

        // To prevent behind locked at 0.000001 or 0.9999
        double m_xmin{-1}, m_xmax{2};
};
}
