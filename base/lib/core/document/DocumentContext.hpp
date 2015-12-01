#pragma once
#include <core/application/ApplicationContext.hpp>
#include <iscore/command/CommandStackFacade.hpp>
namespace iscore
{
class Document;
class CommandStack;
class SelectionStack;
class ObjectLocker;

struct DocumentContext
{
        DocumentContext(iscore::Document& d);

        iscore::ApplicationContext app;
        iscore::Document& document;
        iscore::CommandStackFacade& commandStack;
        iscore::SelectionStack& selectionStack;
        iscore::ObjectLocker& objectLocker;
};
// TODO DocumentComponents : model, pluginmodels...
}
