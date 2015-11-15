#pragma once
namespace iscore
{
class Document;
class CommandStack;
class SelectionStack;
class ObjectLocker;

struct DocumentContext
{
        DocumentContext(iscore::Document& d);

        iscore::Document& document;
        iscore::CommandStack& commandStack;
        iscore::SelectionStack& selectionStack;
        iscore::ObjectLocker& objectLocker;
};

}
