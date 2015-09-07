#pragma once

/*
This file is used to define simple data structure to simplify the code when needed
*/

#include <QVector>
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/TimeValue.hpp>

class TimeNodeModel;
class ConstraintModel;

struct TimenodeProperties {
    TimeValue oldDate;
    TimeValue newDate;
};

struct ConstraintProperties {
    TimeValue oldMin;
    TimeValue newMin;
    TimeValue oldMax;
    TimeValue newMax;
};

struct ElementsProperties {
    QMap<Id<TimeNodeModel>, TimenodeProperties> timenodes;
    QMap<Id<ConstraintModel>, ConstraintProperties> constraints;
};
