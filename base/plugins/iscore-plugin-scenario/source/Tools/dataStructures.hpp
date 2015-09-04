#pragma once

/*
This file is used to define simple data structure to simplify the code when needed
*/

#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

struct TimenodeProperties {
    Id<TimeNodeModel> id;
    TimeValue oldDate;
    TimeValue newDate;
};

struct ConstraintProperties {
    Id<ConstraintModel> id;
    TimeValue oldMin;
    TimeValue newMin;
    TimeValue oldMax;
    TimeValue newMax;
};

struct ElementsProperties {
    QVector<TimenodeProperties> timenodes;
    QVector<ConstraintProperties> constraints;
};
