#pragma once

/*
This file is used to define simple data structure to simplify the code when needed
*/

#include <Process/TimeValue.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QByteArray>
#include <QMap>
#include <QPair>
#include <limits>

namespace Scenario
{
class ConstraintModel;
class ConstraintViewModel;
class RackModel;
class TimeNodeModel;

struct Branch {
        QVector<Id<TimeNodeModel>> timenodes;
        QVector<Id<ConstraintModel>> constraints;
};

enum class TokenState{Disabled, Forward, Backward};

struct Token {
        // deltas are "new - old"
        // so deltaMin used to be positive
        // and deltaMax negative

        Token() {}

        void disable() {
            state = TokenState::Disabled;
            deltaMin = 0;
            deltaMax = 0;
        }

        double deltaMin{0};
        double deltaMax{0};
        TokenState state{TokenState::Disabled};
};

struct TimenodeProperties {

    TimeValue oldDate;
    TimeValue newDate;
    double dateMin;
    double dateMax;

    // CSP on execution purpose :
    double newAbsoluteMin{0};
    double newAbsoluteMax{std::numeric_limits<double>::infinity()};
    QVector<Branch> previousBranches{};

    Token token;
    bool hasToken() {return token.state!= TokenState::Disabled;}
};

struct ConstraintProperties {
    TimeValue oldMin;
    TimeValue newMin;
    TimeValue oldMax;
    TimeValue newMax;
    QPair<
        QPair<
            Path<ConstraintModel>,
            QByteArray
        >, // The constraint data
        QMap< // Mapping for the view models of this constraint
            Id<ConstraintViewModel>,
            Id<RackModel>
        >
     > savedDisplay;

    // CSP on execution purpose
    Token token;
    double min;
    double max;
    double cspMin;
    double cspMax;
    QVector<Branch> previousBranches{};

    bool hasToken() {return token.state!= TokenState::Disabled;}
};

struct ElementsProperties {
    QMap<Id<TimeNodeModel>, TimenodeProperties> timenodes;
    QMap<Id<ConstraintModel>, ConstraintProperties> constraints;
};
}
