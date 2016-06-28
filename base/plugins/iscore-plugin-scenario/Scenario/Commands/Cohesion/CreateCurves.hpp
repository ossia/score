#pragma once
#include <QList>
namespace iscore
{
class CommandStackFacade;
struct DocumentContext;
}
namespace Scenario
{
class ConstraintModel;
}

void CreateCurves(
        const QList<const Scenario::ConstraintModel*>& selected_constraints,
        const iscore::CommandStackFacade& stack);
