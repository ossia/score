#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>

#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePalette.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <QStateMachine>
#include <QPointF>
#include <QVector>

class DynamicCurveSegmentList;
namespace Curve
{
class ToolPalette;
}
class CurveModel;
class CurveView;
class CurvePointView;
class CurveSegmentView;
class QAction;
class QMenu;
class QActionGroup;

class CurvePresenter : public QObject
{
        Q_OBJECT
    public:
        CurvePresenter(
                const iscore::DocumentContext& lst,
                const Curve::Style&,
                const CurveModel&,
                CurveView*,
                QObject* parent);
        virtual ~CurvePresenter();

        const auto& points() const
        { return m_points; }
        const auto& segments() const
        { return m_segments; }

        // Removes all the points & segments
        void clear();

        const CurveModel& model() const
        { return m_model; }
        CurveView& view() const
        { return *m_view; }

        void setRect(const QRectF& rect);

        void enableActions(bool);

        // Changes the colors
        void enable();
        void disable();

        Curve::EditionSettings& editionSettings()
        { return m_editionSettings; }

        void fillContextMenu(
                QMenu*,
                const QPoint&,
                const QPointF&);

        void removeSelection();

    signals:
        void contextMenuRequested(const QPoint&, const QPointF&);

    private:
        // Context menu actions
        void updateSegmentsType(const CurveSegmentFactoryKey& segment);

        // Setup utilities
        void setPos(CurvePointView&);
        void setPos(CurveSegmentView&);
        void setupSignals();
        void setupView();
        void setupStateMachine();

        // Adding
        void addPoint(CurvePointView*);
        void addSegment(CurveSegmentView*);

        void addPoint_impl(CurvePointView*);
        void addSegment_impl(CurveSegmentView*);

        void setupPointConnections(CurvePointView*);
        void setupSegmentConnections(CurveSegmentView*);

        void modelReset();

        const DynamicCurveSegmentList& m_curveSegments;
        Curve::EditionSettings m_editionSettings;

        const CurveModel& m_model;
        CurveView* m_view{};

        IdContainer<CurvePointView, CurvePointModel> m_points;
        IdContainer<CurveSegmentView, CurveSegmentModel> m_segments;

        // Required dispatchers
        CommandDispatcher<> m_commandDispatcher;
        iscore::SelectionDispatcher m_selectionDispatcher;

        const Curve::Style& m_style;

        QMenu* m_contextMenu{};
        QActionGroup* m_actions{};

        bool m_enabled = true;
};
