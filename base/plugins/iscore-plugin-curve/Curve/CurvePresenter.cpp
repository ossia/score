#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveView.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Segment/CurveSegmentView.hpp"
#include "Curve/Segment/CurveSegmentList.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Point/CurvePointView.hpp"
#include "Curve/StateMachine/OngoingCommandState.hpp"

#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"
#include "Curve/StateMachine/States/Tools/MoveTool.hpp"

#include "Curve/Commands/UpdateCurve.hpp"

#include <QGraphicsScene>
#include <QActionGroup>
#include <QKeyEvent>
#include <QMenu>

CurvePresenter::CurvePresenter(CurveModel* model, CurveView* view, QObject* parent):
    QObject{parent},
    m_model{model},
    m_view{view},
    m_commandDispatcher{iscore::IDocument::documentFromObject(*model)->commandStack()},
    m_selectionDispatcher{iscore::IDocument::documentFromObject(*model)->selectionStack()}
{
    // For each segment in the model, create a segment and relevant points in the view.
    // If the segment is linked to another, the point is shared.
    setupView();
    setupContextMenu();
    setupSignals();

    m_sm = new CurveStateMachine{*this, this};
}

CurvePresenter::~CurvePresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

CurveModel* CurvePresenter::model() const
{
    return m_model;
}

CurveView& CurvePresenter::view() const
{
    return *m_view;
}

QPointF myscale(const QPointF& first, const QSizeF& second)
{
    return {first.x() * second.width(), (1. - first.y()) * second.height()};
}

void CurvePresenter::setRect(const QRectF& rect)
{
    m_view->setRect(rect);

    // Positions
    for(CurvePointView* curve_pt : m_points)
    {
        setPos(curve_pt);
    }

    for(CurveSegmentView* curve_segt : m_segments)
    {
        setPos(curve_segt);
    }
}

void CurvePresenter::setPos(CurvePointView * point)
{
    auto size = m_view->boundingRect().size();
    // Get the previous or next segment. There has to be at least one.
    if(point->model().previous())
    {
        auto& curvemodel = *m_model->segments().find(point->model().previous());
        point->setPos(myscale(curvemodel->end(), size));
    }
    else if(point->model().following())
    {
        auto& curvemodel = *m_model->segments().find(point->model().following());
        point->setPos(myscale(curvemodel->start(), size));
    }
}

void CurvePresenter::setPos(CurveSegmentView * segment)
{
    auto rect = m_view->boundingRect();
    // Pos is the top-left corner of the segment
    // Width is from begin to end
    // Height is the height of the curve since the segment can do anything in-between.
    double startx, endx;
    startx = segment->model().start().x() * rect.width();
    endx = segment->model().end().x() * rect.width();
    segment->setPos({startx, 0});
    segment->setRect({0., 0., endx - startx, rect.height()});
}

void CurvePresenter::setupSignals()
{
    connect(m_model, &CurveModel::segmentAdded, this,
            [&] (CurveSegmentModel* segment)
    {
        addSegment(new CurveSegmentView{segment, m_view});
    });

    connect(m_model, &CurveModel::pointAdded, this,
            [&] (CurvePointModel* point)
    {
        addPoint(new CurvePointView{point, m_view});
    });

    connect(m_model, &CurveModel::pointRemoved, this,
            [&] (CurvePointModel* m)
    {
        auto it = std::find_if(
                      m_points.begin(),
                      m_points.end(),
                      [&] (CurvePointView* pt) { return &pt->model() == m; });

        if(it != m_points.end()) // TODO should never happen ?
        {
            auto val = *it;
            m_points.removeOne(val);

            delete val;
        }
    });
    connect(m_model, &CurveModel::segmentRemoved, this,
            [&] (CurveSegmentModel* m)

    {
        auto it = std::find_if(
                      m_segments.begin(),
                      m_segments.end(),
                      [&] (CurveSegmentView* segment) { return &segment->model() == m; });

        if(it != m_segments.end())
        {
            auto val = *it;
            m_segments.removeOne(val);

            delete val;
        }
    });

    connect(m_model, &CurveModel::cleared, this,
            [&] ()
    {
        qDeleteAll(m_points);
        qDeleteAll(m_segments);
        m_points.clear();
        m_segments.clear();
    });
}

void CurvePresenter::setupView()
{
    // Initialize the elements
    for(CurveSegmentModel* segment : m_model->segments())
    {
        addSegment(new CurveSegmentView{segment, m_view});
    }

    for(CurvePointModel* pt : m_model->points())
    {
        addPoint(new CurvePointView{pt, m_view});
    }

    // Setup the actions
    m_actions = new QActionGroup{this};
    m_actions->setExclusive(true);
    auto moveact = new QAction{m_actions};
    moveact->setCheckable(true);
    moveact->setChecked(true);
    m_actions->addAction(moveact);

    auto shiftact = new QAction{m_actions};
    shiftact->setCheckable(true);
    m_actions->addAction(shiftact);

    auto ctrlact = new QAction{m_actions};
    ctrlact->setCheckable(true);
    m_actions->addAction(ctrlact);

    m_actions->setEnabled(true);

    connect(moveact, &QAction::toggled, this, [&] (bool b) {
        if(b)
        {
            m_sm->changeTool((int)Curve::Tool::Move);
        }
    });
    connect(shiftact, &QAction::toggled, this, [&] (bool b) {
        if(b)
        {
            m_sm->changeTool((int)Curve::Tool::SetSegment);
        }
        else
        {
            m_sm->changeTool((int)Curve::Tool::Move);
        }
    });
    connect(ctrlact, &QAction::toggled, this, [&] (bool b) {
        if(b)
        {
            m_sm->changeTool((int)Curve::Tool::Create);
        }
        else
        {
            m_sm->changeTool((int)Curve::Tool::Move);
        }
    });

    connect(m_view, &CurveView::keyPressed, this, [=] (int key)
    {
        if(key == Qt::Key_L)
            m_sm->changeTool(!m_sm->tool());

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
            moveact->setChecked(true);
        }
        if(key == Qt::Key_Control)
        {
            moveact->setChecked(true);
        }
    });

}

void CurvePresenter::setupStateMachine()
{
}

void CurvePresenter::setupContextMenu()
{
    m_contextMenu = new QMenu;

    auto selectAct = new QAction{tr("Select"), this};

    selectAct->setCheckable(true);
    selectAct->setChecked(false);
    connect(selectAct, &QAction::toggled, this,
            [&] (bool b) {
        if(b)
            m_sm->changeTool(int(Curve::Tool::Selection));
        else
            m_sm->changeTool(int(Curve::Tool::Move));
    });

    auto removeAct = new QAction{tr("Remove"), this};
    removeAct->setData(2); // Small identifier for segments actions...

    auto typeMenu = m_contextMenu->addMenu(tr("Type"));
    for(const auto& seg : SingletonCurveSegmentList::instance().nameList())
    {
        auto act = typeMenu->addAction(seg);
        act->setData(1);
    }

    auto lockAction = new QAction{tr("Lock between points"), this};
    connect(lockAction, &QAction::toggled,
            this, [&] (bool b) { setLockBetweenPoints(b); });
    lockAction->setCheckable(true);
    lockAction->setChecked(true);

    auto suppressAction = new QAction{tr("Suppress on overlap"), this};
    connect(suppressAction, &QAction::toggled,
            this, [&] (bool b) { setSuppressOnOverlap(b); });

    suppressAction->setCheckable(true);
    suppressAction->setChecked(false);

    m_contextMenu->addAction(selectAct);
    m_contextMenu->addAction(removeAct);
    m_contextMenu->addAction(lockAction);
    m_contextMenu->addAction(suppressAction);

    connect(m_view, &CurveView::contextMenuRequested,
            this, [&] (const QPoint& pt)
    {
        auto act = m_contextMenu->exec(pt, nullptr);
        m_contextMenu->close();

        if(!act)
        {
            return;
        }
        else if(act->data().value<int>() == 1)
        {
            updateSegmentsType(act->text());
        }
        else if(act->data().value<int>() == 2)
        {
            removeSelection();
        }
    });
}

void CurvePresenter::addPoint(CurvePointView * pt_view)
{
    connect(pt_view, &CurvePointView::contextMenuRequested,
            m_view, &CurveView::contextMenuRequested);
    m_points.push_back(pt_view);

    setPos(pt_view);
}

void CurvePresenter::addSegment(CurveSegmentView * seg_view)
{
    connect(seg_view, &CurveSegmentView::contextMenuRequested,
            m_view, &CurveView::contextMenuRequested);
    m_segments.push_back(seg_view);

    setPos(seg_view);
}

CurvePresenter::AddPointBehaviour CurvePresenter::addPointBehaviour() const
{
    return m_addPointBehaviour;
}

void CurvePresenter::setAddPointBehaviour(const AddPointBehaviour &addPointBehaviour)
{
    m_addPointBehaviour = addPointBehaviour;
}

void CurvePresenter::enableActions(bool b)
{
    m_actions->setEnabled(b);
}

void CurvePresenter::removeSelection()
{
    // We remove all that is selected,
    // And set the bounds correctly
    QSet<id_type<CurveSegmentModel>> segmentsToDelete;

    for(const auto& elt : m_model->selectedChildren())
    {
        if(auto point = dynamic_cast<const CurvePointModel*>(elt))
        {
            if(point->previous())
                segmentsToDelete.insert(point->previous());
            if(point->following())
                segmentsToDelete.insert(point->following());
        }

        if(auto segmt = dynamic_cast<const CurveSegmentModel*>(elt))
        {
            segmentsToDelete.insert(segmt->id());
        }
    }

    QVector<QByteArray> newSegments;
    newSegments.resize(m_model->segments().size() - segmentsToDelete.size());
    int i = 0;
    for(const CurveSegmentModel* segment : m_model->segments())
    {
        if(!segmentsToDelete.contains(segment->id()))
        {
            auto cp = segment->clone(segment->id(), nullptr);
            if(segment->previous() && !segmentsToDelete.contains(segment->previous()))
                cp->setPrevious(segment->previous());
            if(segment->following() && !segmentsToDelete.contains(segment->following()))
                cp->setFollowing(segment->following());

            Serializer<DataStream> s{&newSegments[i++]};
            s.readFrom(*cp);
        }
    }

    m_commandDispatcher.submitCommand(
                new UpdateCurve{
                    iscore::IDocument::path(m_model),
                    std::move(newSegments)
                });
}

void CurvePresenter::updateSegmentsType(const QString& segmentName)
{
    // They keep their start / end and previous / following but change type.
    // TODO maybe it would be better to encapsulate this ?
    auto factory = SingletonCurveSegmentList::instance().get(segmentName);

    QVector<QByteArray> newSegments;
    newSegments.resize(m_model->segments().size());
    int i = 0;
    for(CurveSegmentModel* segment : m_model->segments())
    {
        CurveSegmentModel* current;
        if(segment->selection.get())
        {
            auto ns = factory->make(segment->id(), nullptr);
            ns->setStart(segment->start());
            ns->setEnd(segment->end());
            ns->setPrevious(segment->previous());
            ns->setFollowing(segment->following());

            current = ns;
        }
        else
        {
            current = segment;
        }

        Serializer<DataStream> s{&newSegments[i++]};
        s.readFrom(*current);
    }

    m_commandDispatcher.submitCommand(
                new UpdateCurve{
                    iscore::IDocument::path(m_model),
                    std::move(newSegments)
                });
}

bool CurvePresenter::stretchBothBounds() const
{
    return m_stretchBothBounds;
}

void CurvePresenter::setStretchBothBounds(bool stretchBothBounds)
{
    m_stretchBothBounds = stretchBothBounds;
}

bool CurvePresenter::suppressOnOverlap() const
{
    return m_suppressOnOverlap;
}

void CurvePresenter::setSuppressOnOverlap(bool suppressOnOverlap)
{
    m_suppressOnOverlap = suppressOnOverlap;
}

void CurvePresenter::setLockBetweenPoints(bool lockBetweenPoints)
{
    m_lockBetweenPoints = lockBetweenPoints;
}

bool CurvePresenter::lockBetweenPoints() const
{
    return m_lockBetweenPoints;
}
