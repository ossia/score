#include <QCoreApplication>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentContext.hpp>

int main()
{
  iscore::Document* doc{};
  iscore::CommandStack* cstack{};
  iscore::SelectionStack* sstack{};
  iscore::ObjectLocker* olck{};
  iscore::FocusManager* focus{};
  iscore::GUIApplicationContext* appctx{};
  QTimer* upd{};
  //iscore::DocumentContext d{*doc, *cstack, *sstack, *olck, *focus, *appctx, *upd};

  //iscore::DocumentModel

}
