#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveView.hpp"
#include "Curve/Segment/CurveSegmentList.hpp"
#include "Curve/StateMachine/OngoingState.hpp"

#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include "Curve/StateMachine/States/Tools/MoveTool.hpp"
#include <Curve/StateMachine/CurveStateMachine.hpp>
#include "Curve/Commands/UpdateCurve.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentContext.hpp>
#include <core/application/ApplicationComponents.hpp>

#include <QGraphicsScene>
#include <QActionGroup>
#include <QKeyEvent>
#include <QMenu>

static QPointF myscale(QPointF first, QSizeF second)
{
    return {first.x() * second.width(), (1. - first.y()) * second.height()};
}

CurvePresenter::CurvePresenter(
        const iscore::DocumentContext& context,
        const Curve::Style& style,
        const CurveModel& model,
        CurveView* view,
        QObject* parent):
    QObject{parent},
    m_curveSegments{context.app.components.factory<DynamicCurveSegmentList>()},
    m_model{model},
    m_view{view},
    m_commandDispatcher{context.commandStack},
    m_selectionDispatcher{context.selectionStack},
    m_style{style}
{
    // For each segment in the model, create a segment and relevant points in the view.
    // If the segment is linked to another, the point is shared.
    setupView();
    setupSignals();

    connect(m_view, &CurveView::contextMenuRequested,
            this, &CurvePresenter::contextMenuRequested);
}

CurvePresenter::~CurvePresenter()
{
    deleteGraphicsObject(m_view);
}

void CurvePresenter::setRect(const QRectF& rect)
{
    m_view->setRect(rect);

    // Positions
    for(auto& curve_pt : m_points)
    {
        setPos(curve_pt);
    }

    for(auto& curve_segt : m_segments)
    {
        setPos(curve_segt);
    }
}

void CurvePresenter::setPos(CurvePointView& point)
{
    point.setPos(myscale(point.model().pos(),  m_view->boundingRect().size()));
}

void CurvePresenter::setPos(CurveSegmentView& segment)
{
    auto rect = m_view->boundingRect();
    // Pos is the top-left corner of the segment
    // Width is from begin to end
    // Height is the height of the curve since the segment can do anything in-between.
    double startx, endx;
    startx = segment.model().start().x() * rect.width();
    endx = segment.model().end().x() * rect.width();
    segment.setPos({startx, 0});
    segment.setRect({0., 0., endx - startx, rect.height()});
}

void CurvePresenter::setupSignals()
{
    con(m_model, &CurveModel::segmentAdded, this,
            [&] (const CurveSegmentModel& segment)
    {
        addSegment(new CurveSegmentView{&segment, m_style, m_view});
    });

    con(m_model, &CurveModel::pointAdded, this,
            [&] (const CurvePointModel& point)
    {
        addPoint(new CurvePointView{&point, m_style, m_view});
    });

    con(m_model, &CurveModel::pointRemoved, this,
            [&] (const Id<CurvePointModel>& m)
    {
        auto& map = m_points.get();
        auto it = map.find(m);
        if(it != map.end()) // TODO should never happen ?
        {
            delete *it;
            map.erase(it);
        }
    });

    con(m_model, &CurveModel::segmentRemoved, this,
            [&] (const Id<CurveSegmentModel>& m)
    {
        auto& map = m_segments.get();
        auto it = map.find(m);
        if(it != map.end()) // TODO should never happen ?
        {
            delete *it;
            map.erase(it);
        }
    });

    con(m_model, &CurveModel::cleared, this,
            [&] ()
    {
        qDeleteAll(m_points.get());
        qDeleteAll(m_segments.get());
        m_points.clear();
        m_segments.clear();
    });

    con(m_model, &CurveModel::curveReset,
        this, &CurvePresenter::modelReset);
}

void CurvePresenter::setupView()
{
    // Initialize the elements
    for(const auto& segment : m_model.segments())
    {
        addSegment(new CurveSegmentView{&segment, m_style, m_view});
    }

    for(CurvePointModel* pt : m_model.points())
    {
        addPoint(new CurvePointView{pt, m_style, m_view});
    }

    // Setup the actions
    m_actions = new QActionGroup{this};
    m_actions->setExclusive(true);

    auto shiftact = new QAction{m_actions};
    shiftact->setCheckable(true);
    m_actions->addAction(shiftact);

    auto ctrlact = new QAction{m_actions};
    ctrlact->setCheckable(true);
    m_actions->addAction(ctrlact);

    m_actions->setEnabled(true);

    connect(shiftact, &QAction::toggled, this, [&] (bool b) {
        if(b)
        {
            editionSettings().setTool(Curve::Tool::SetSegment);
        }
        else
        {
            editionSettings().setTool(Curve::Tool::Select);
        }
    });
    connect(ctrlact, &QAction::toggled, this, [&] (bool b) {
        if(b)
        {
            editionSettings().setTool(Curve::Tool::Create);
        }
        else
        {
            editionSettings().setTool(Curve::Tool::Select);
        }
    });

    connect(m_view, &CurveView::keyPressed, this, [=] (int key)
    {
        if(key == Qt::Key_Shift)
        {
            shiftact->setChecked(true);
        }
        if(key == Qt::Key_Control)
        {
            ctrlact->setChecked(true);
        }
    });

    connect(m_view, &CurveView::keyReleased, this, [=] (int key)
    {
        if(key == Qt::Key_Shift)
        {
            shiftact->setChecked(false);
            editionSettings().setTool(Curve::Tool::Select);
        }
        if(key == Qt::Key_Control)
        {
            ctrlact->setChecked(false);
            editionSettings().setTool(Curve::Tool::Select);
        }
    });
}

void CurvePresenter::fillContextMenu(
        QMenu* menu,
        const QPoint& pos,
        const QPointF& scenepos)
{
    menu->addSeparator();

    auto removeAct = new QAction{tr("Remove"), this};
    connect(removeAct, &QAction::triggered,
            [&] () {
        removeSelection();
    });

    auto typeMenu = menu->addMenu(tr("Type"));
    for(const auto& seg : m_curveSegments.list().get())
    {
        auto act = typeMenu->addAction(seg.second->prettyName());
        connect(act, &QAction::triggered,
                this, [=] () {
            updateSegmentsType(seg.first);
        });
    }

    auto lockAction = new QAction{tr("Lock between points"), this};
    connect(lockAction, &QAction::toggled,
            this, [&] (bool b) { m_editionSettings.setLockBetweenPoints(b); });
    lockAction->setCheckable(true);
    lockAction->setChecked(m_editionSettings.lockBetweenPoints());

    auto suppressAction = new QAction{tr("Suppress on overlap"), this};
    connect(suppressAction, &QAction::toggled,
            this, [&] (bool b) { m_editionSettings.setSuppressOnOverlap(b); });

    suppressAction->setCheckable(true);
    suppressAction->setChecked(m_editionSettings.suppressOnOverlap());

    menu->addAction(removeAct);
    menu->addAction(lockAction);
    menu->addAction(suppressAction);
}

void CurvePresenter::addPoint(CurvePointView * pt_view)
{
    setupPointConnections(pt_view);
    addPoint_impl(pt_view);
}

void CurvePresenter::addSegment(CurveSegmentView * seg_view)
{
    setupSegmentConnections(seg_view);
    addSegment_impl(seg_view);
}

void CurvePresenter::addPoint_impl(CurvePointView* pt_view)
{
    m_points.insert(pt_view);
    setPos(*pt_view);

    m_enabled ? pt_view->enable() : pt_view->disable();
}

void CurvePresenter::addSegment_impl(CurveSegmentView* seg_view)
{
    m_segments.insert(seg_view);
    setPos(*seg_view);

    m_enabled ? seg_view->enable() : seg_view->disable();
}

void CurvePresenter::setupPointConnections(CurvePointView* pt_view)
{
    connect(pt_view, &CurvePointView::contextMenuRequested,
            m_view, &CurveView::contextMenuRequested);
    con(pt_view->model(), &CurvePointModel::posChanged,
        this, [=] () { setPos(*pt_view); });
}

void CurvePresenter::setupSegmentConnections(CurveSegmentView* seg_view)
{
    connect(seg_view, &CurveSegmentView::contextMenuRequested,
            m_view, &CurveView::contextMenuRequested);
}

void CurvePresenter::modelReset()
{
    // 1. We put our current elements in our pool.
    std::vector<CurvePointView*> points(m_points.get().begin(), m_points.get().end());
    std::vector<CurveSegmentView*> segments(m_segments.get().begin(), m_segments.get().end());

    std::vector<CurvePointView*> newPoints;
    std::vector<CurveSegmentView*> newSegments;

    // 2. We add / remove new elements if necessary
    {
        int diff_points = m_model.points().size() - points.size();
        if(diff_points > 0)
        {
            points.reserve(points.size() + diff_points);
            for(;diff_points --> 0;)
            {
                auto pt = new CurvePointView{nullptr, m_style, m_view};
                points.push_back(pt);
                newPoints.push_back(pt);
            }
        }
        else if(diff_points < 0)
        {
            int inv_diff_points = -diff_points;
            for(;inv_diff_points --> 0;)
            {
                deleteGraphicsObject(points[points.size() - inv_diff_points - 1]);
            }
            points.resize(points.size() + diff_points);
        }
    }

    // Same for segments
    {
        int diff_segts = m_model.segments().size() - segments.size();
        if(diff_segts > 0)
        {
            segments.reserve(segments.size() + diff_segts);
            for(;diff_segts --> 0;)
            {
                auto seg = new CurveSegmentView{nullptr, m_style, m_view};
                segments.push_back(seg);
                newSegments.push_back(seg);
            }
        }
        else if(diff_segts < 0)
        {
            int inv_diff_segts = -diff_segts;
            for(;inv_diff_segts --> 0;)
            {
                deleteGraphicsObject(segments[segments.size() - inv_diff_segts - 1]);
            }
            segments.resize(segments.size() + diff_segts);
        }
    }

    ISCORE_ASSERT((int)points.size() == m_model.points().size());
    ISCORE_ASSERT(segments.size() == m_model.segments().size());

    // 3. We set the data
    { // Points
        int i = 0;
        for(const auto& point : m_model.points())
        {
            points.at(i)->setModel(point);
            i++;
        }
    }
    { // Segments
        int i = 0;
        for(const auto& segment : m_model.segments())
        {
            segments[i]->setModel(&segment);
            i++;
        }
    }

    for(const auto& seg : newSegments)
        setupSegmentConnections(seg);
    for(const auto& pt: newPoints)
        setupPointConnections(pt);

    // Now the ones that have a new model
    // 4. We put them all back in our maps.
    m_points.clear();
    m_segments.clear();

    for(const auto& pt_view : points)
    {
        addPoint_impl(pt_view);
    }
    for(const auto& seg_view : segments)
    {
        addSegment_impl(seg_view);
    }
}

void CurvePresenter::enableActions(bool b)
{
    m_actions->setEnabled(b);
}

void CurvePresenter::enable()
{
    for(auto& segment : m_segments)
    {
        segment.enable();
    }
    for(auto& point : m_points)
    {
        point.enable();
    }

    m_enabled = true;
}

void CurvePresenter::disable()
{
    for(auto& segment : m_segments)
    {
        segment.disable();
    }
    for(auto& point : m_points)
    {
        point.disable();
    }

    m_enabled = false;
}

// TESTME
void CurvePresenter::removeSelection()
{
    // We remove all that is selected,
    // And set the bounds correctly
    QSet<Id<CurveSegmentModel>> segmentsToDelete;

    for(const auto& elt : m_model.selectedChildren())
    {
        if(auto point = dynamic_cast<const CurvePointModel*>(elt.data()))
        {
            if(point->previous())
                segmentsToDelete.insert(point->previous());
            if(point->following())
                segmentsToDelete.insert(point->following());
        }

        if(auto segmt = dynamic_cast<const CurveSegmentModel*>(elt.data()))
        {
            segmentsToDelete.insert(segmt->id());
        }
    }

    auto newSegments = model().toCurveData();
    auto it = newSegments.begin();
    while(it != newSegments.end())
    {
        if(segmentsToDelete.contains(it->id))
        {
            it = newSegments.erase(it);
            continue;
        }

        if(it->previous && segmentsToDelete.contains(it->previous))
            it->previous = Id<CurveSegmentModel>{};
        if(it->following && segmentsToDelete.contains(it->following))
            it->following = Id<CurveSegmentModel>{};

        it++;
    }

    m_commandDispatcher.submitCommand(
                new UpdateCurve{
                    m_model,
                    std::move(newSegments)
                });
}

void CurvePresenter::updateSegmentsType(const CurveSegmentFactoryKey& segment)
{
    // They keep their start / end and previous / following but change type.
    auto factory = m_curveSegments.list().get(segment);
    auto this_type_base_data = factory->makeCurveSegmentData();
    auto newSegments = model().toCurveData();

    for(auto& seg_data : newSegments)
    {
        if(model().segments().at(seg_data.id).selection.get())
        {
            seg_data.type = segment;
            seg_data.specificSegmentData = this_type_base_data;
        }
    }

    m_commandDispatcher.submitCommand(
                new UpdateCurve{
                    m_model,
                    std::move(newSegments)
                });
}
