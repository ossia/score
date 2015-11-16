#pragma once
#include <core/application/ApplicationContext.hpp>
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
        iscore::CommandStack& commandStack;
        iscore::SelectionStack& selectionStack;
        iscore::ObjectLocker& objectLocker;
};

}
