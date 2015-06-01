#pragma once
#include <QVector>
#include <QPointF>
class CurvePresenter;


/*
class CommandObject
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
// TODO do automation where only points are sent.
class CurveSegmentModel;
class CurveCommandObjectBase
{
    public:
        CurveCommandObjectBase(CurvePresenter* pres);
        void press();

    protected:
        virtual void on_press() = 0;

        QVector<CurveSegmentModel*> m_originalSegments;
        QPointF m_originalPress; // Note : there should be only one per curve...

    private:
        CurvePresenter* m_presenter{};
};
