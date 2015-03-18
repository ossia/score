#pragma once

#include <QObject>
#include "ProcessInterface/TimeValue.hpp"

class TimeRulerView;

class TimeRulerPresenter : public QObject
{
        Q_OBJECT
    public:
        explicit TimeRulerPresenter(TimeRulerView* view, QObject *parent = 0);
        ~TimeRulerPresenter();

        TimeRulerView* view() { return m_view; }

    signals:

    public slots:
        void setDuration(TimeValue dur);

    private:
        TimeRulerView* m_view;
};
