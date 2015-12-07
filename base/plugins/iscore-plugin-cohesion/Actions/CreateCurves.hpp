#pragma once
#include <QList>
class ConstraintModel;
namespace iscore
{
class CommandStackFacade;
struct DocumentContext;
}


void CreateCurves(
        const QList<const ConstraintModel*>& selected_constraints,
        iscore::CommandStackFacade& stack);
