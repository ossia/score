#pragma once
#include <QList>

namespace iscore
{
class CommandStackFacade;
class Document;
}
class ConstraintModel;

void InterpolateStates(iscore::Document* doc);
void InterpolateStates(const QList<const ConstraintModel*>&,
                       iscore::CommandStackFacade&);
