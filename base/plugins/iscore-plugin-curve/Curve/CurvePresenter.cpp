#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveView.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include "Curve/Segment/CurveSegmentView.hpp"
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/Point/CurvePointView.hpp"
#include <iscore/command/OngoingCommandManager.hpp>
#include "Curve/StateMachine/OngoingCommandState.hpp"
#include <QKeyEvent>
#include <QMenu>
#include <QAction>

#include "Curve/StateMachine/CommandObjects/MovePointCommandObject.hpp"

#include "Curve/Commands/UpdateCurve.hpp"
#include "StateMachine/CurveStateMachine.hpp"

CurvePresenter::CurvePresenter(CurveModel* model, CurveView* view):
    m_model{model},
    m_view{view},
    m_commandDispatcher{iscore::IDocument::documentFromObject(*model)->commandStack()},
    m_selectionDispatcher{iscore::IDocument::documentFromObject(*model)->selectionStack()}
{
    // For each segment in the model, create a segment and relevant points in the view.
    // If the segment is linked to another, the point is shared.
    setupView();
    setupSignals();
    m_sm = new CurveStateMachine(*this, this);
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
        // Create a segment
        auto seg_view = new CurveSegmentView{segment, m_view};
        connect(seg_view, &CurveSegmentView::contextMenuRequested,
                m_view, &CurveView::contextMenuRequested);
        m_segments.push_back(seg_view);

        setPos(seg_view);
    });

    connect(m_model, &CurveModel::pointAdded, this,
            [&] (CurvePointModel* point)
    {
        // Create a segment
        auto pt_view = new CurvePointView{point, m_view};
        connect(pt_view, &CurvePointView::contextMenuRequested,
                m_view, &CurveView::contextMenuRequested);
        m_points.push_back(pt_view);

        setPos(pt_view);
    });

    connect(m_model, &CurveModel::pointRemoved, this,
            [&] (CurvePointModel* m)
    {
        auto it = std::find_if(
                      m_points.begin(),
                      m_points.end(),
                      [&] (CurvePointView* pt) { return &pt->model() == m; });
        auto val = *it;

        m_points.removeOne(val);
        delete val;
    });
    connect(m_model, &CurveModel::segmentRemoved, this,
            [&] (CurveSegmentModel* m)

    {
        auto it = std::find_if(
                      m_segments.begin(),
                      m_segments.end(),
                      [&] (CurveSegmentView* segment) { return &segment->model() == m; });
        auto val = *it;

        m_segments.removeOne(val);
        delete val;
    });
}

void CurvePresenter::setupView()
{
    for(CurveSegmentModel* segment : m_model->segments())
    {
        // Create a segment
        auto seg_view = new CurveSegmentView{segment, m_view};
        m_segments.push_back(seg_view);
    }

    for(CurvePointModel* pt : m_model->points())
    {
        // Create a point
        auto pt_view = new CurvePointView{pt, m_view};
        m_points.push_back(pt_view);
    }

    connect(m_view, &CurveView::keyPressed, this, [&] (int key)
    {
        if(key == Qt::Key_L)
            m_sm->changeTool(!m_sm->tool());
    });
    connect(m_view, &CurveView::keyReleased, this, [&] (int key)
    {
    });

    connect(m_view, &CurveView::contextMenuRequested,
            this, [&] (const QPoint& pt)
    {
        QMenu m;
        auto removeAct = new QAction(tr("Remove"), this);
        m.addAction(removeAct);

        auto act = m.exec(pt, nullptr);
        if(act == removeAct)
        {
            removeSelection();
        }
    });
}

void CurvePresenter::setupStateMachine()
{
    m_sm->changeTool(0);
}

CurvePresenter::AddPointBehaviour CurvePresenter::addPointBehaviour() const
{
    return m_addPointBehaviour;
}

void CurvePresenter::setAddPointBehaviour(const AddPointBehaviour &addPointBehaviour)
{
    m_addPointBehaviour = addPointBehaviour;
}

void CurvePresenter::removeSelection()
{
    // First we deselect
    m_selectionDispatcher.setAndCommit({});

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

            Serializer<DataStream> s(&newSegments[i++]);
            s.readFrom(*cp);
        }
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


