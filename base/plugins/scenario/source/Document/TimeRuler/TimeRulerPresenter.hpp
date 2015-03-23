#pragma once

#include <Document/TimeRuler/AbstractTimeRuler.hpp>
#include "ProcessInterface/TimeValue.hpp"

class TimeRulerView;

class TimeRulerPresenter : public AbstractTimeRuler
{
        Q_OBJECT
    public:
        explicit TimeRulerPresenter(TimeRulerView* view, QObject *parent = 0);
        ~TimeRulerPresenter();

    signals:

    public slots:

    private:

};
