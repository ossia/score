#pragma once
#include <QStateMachine>
#include <QPointF>
#include <QVector>

class CurveModel;
class CurveView;
class CurvePointView;
class CurveSegmentView;
class CurvePresenter : public QObject
{
        Q_OBJECT
    public:
        enum class AddPointBehaviour
        { LinearBefore, LinearAfter, DuplicateSegment };
        CurvePresenter(CurveModel*, CurveView*);

        CurveModel* model() const;
        CurveView& view() const;

        // Taken from the view. First set this,
        // then send signals to the state machine.
        QPointF pressedPoint() const;

        void setRect(const QRectF& rect);

    private:
        void setupView();
        void setupStateMachine();
        QStateMachine* m_sm{};

        // Data relative to the current state of the view
        QPointF m_currentScenePoint;
        CurveSegmentView* m_currentSegmentView{};
        CurvePointView* m_currentPointView{};

        CurveModel* m_model{};
        CurveView* m_view{};

        QVector<CurvePointView*> m_points;
        QVector<CurveSegmentView*> m_segments;

        // Boolean values that keep the editing state. Should they go here ?
        // Maybe in the settings, instead ?
        bool m_lockBetweenPoints{};
        bool m_suppressOnOverlap{};
        bool m_stretchBothBounds{};
        AddPointBehaviour m_addPointBehaviour{};
};
