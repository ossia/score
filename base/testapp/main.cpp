#include <core/application/Application.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <QApplication>
#include <core/document/Document.hpp>
#include <core/command/CommandStack.hpp>
int main(int argc, char** argv)
{
    iscore::TestApplication app(argc, argv);
    auto& ctx = app.context();
    auto doc = ctx.documents.loadStack(ctx, "stacks/stack.stack");
    iscore::CommandStack& stack = doc->commandStack();
    while(stack.canUndo())
    {
        stack.undo();
    }
    while(stack.canRedo())
    {
        stack.redo();
    }

    auto byte_arr = doc->saveAsByteArray();
    auto json_arr = doc->saveAsJson();

    auto doctype = *ctx.components.availableDocuments().begin();
    ctx.documents.loadDocument(ctx, byte_arr, doctype);
    ctx.documents.loadDocument(ctx, json_arr, doctype);

    return 0;
    //return app.exec();
}
