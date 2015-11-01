#pragma once
#include <QList>
namespace iscore
{
class Document;
class CommandStack;
}
class StateModel;
// TODO RENAMEME

void RefreshStates(iscore::Document* doc);
void RefreshStates(const QList<const StateModel*>&, iscore::CommandStack&);
