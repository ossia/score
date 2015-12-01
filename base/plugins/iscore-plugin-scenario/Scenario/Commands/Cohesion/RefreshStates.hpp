#pragma once
#include <QList>

namespace iscore
{
class CommandStackFacade;
class Document;
}
class StateModel;

void RefreshStates(iscore::Document* doc);
void RefreshStates(const QList<const StateModel*>&,
                   iscore::CommandStackFacade&);
