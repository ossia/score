#pragma once
#include <QList>
namespace iscore
{
class Document;
class CommandStack;
}
class ConstraintModel;
void InterpolateStates(iscore::Document* doc);
void InterpolateStates(const QList<const ConstraintModel*>&, iscore::CommandStack&);
