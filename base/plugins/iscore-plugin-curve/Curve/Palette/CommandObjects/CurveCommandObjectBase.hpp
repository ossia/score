#pragma once
#include <QVector>
#include <QPointF>
#include "Curve/Palette/CurvePaletteBaseStates.hpp"
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include "Curve/Commands/UpdateCurve.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
class CurvePresenter;

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
class CurveSegmentModel;
class CurveCommandObjectBase
{
    public:
        CurveCommandObjectBase(CurvePresenter* pres, iscore::CommandStack&);
        virtual ~CurveCommandObjectBase();

        void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }
        void press();


        void handleLocking();

        // Creates and pushes an UpdateCurve command
        // from a vector of segments.
        // They are removed afterwards
        void submit(std::vector<CurveSegmentData>&&);

    protected:

        auto find(
                std::vector<CurveSegmentData>& segments,
                const Id<CurveSegmentModel>& id)
        {
            return std::find_if(
                        segments.begin(),
                        segments.end(),
                        [&] (const auto& seg) { return seg.id == id; });
        }
        auto find(
                const std::vector<CurveSegmentData>& segments,
                const Id<CurveSegmentModel>& id)
        {
            return std::find_if(
                        segments.cbegin(),
                        segments.cend(),
                        [&] (const auto& seg) { return seg.id == id; });
        }

        template<typename Container>
        Id<CurveSegmentModel> getSegmentId(const Container& ids)
        {
            Id<CurveSegmentModel> id {};

            do
            {
                id = Id<CurveSegmentModel>{iscore::random_id_generator::getRandomId()};
            }
            while(ids.find(id) != ids.end());

            return id;
        }

        Id<CurveSegmentModel> getSegmentId(const std::vector<CurveSegmentData>& ids)
        {
            Id<CurveSegmentModel> id {};

            do
            {
                id = Id<CurveSegmentModel>{iscore::random_id_generator::getRandomId()};
            }
            while(std::find_if(ids.begin(),
                               ids.end(),
                               [&] (const auto& other) { return other.id == id; }) != ids.end());

            return id;
        }

        virtual void on_press() = 0;

        QVector<QByteArray> m_oldCurveData;
        QPointF m_originalPress; // Note : there should be only one per curve...

        CurvePresenter* m_presenter{};

        Curve::StateBase* m_state{};

        SingleOngoingCommandDispatcher<UpdateCurve> m_dispatcher;
        Path<CurveModel> m_modelPath;

        std::vector<CurveSegmentData> m_startSegments;

        // To prevent behind locked at 0.000001 or 0.9999
        double m_xmin{-1}, m_xmax{2};
};
