#pragma once
#include <QList>

namespace iscore
{
class CommandStack;
class Document;
}
class ConstraintModel;

void InterpolateStates(iscore::Document* doc);
void InterpolateStates(const QList<const ConstraintModel*>&, iscore::CommandStack&);
