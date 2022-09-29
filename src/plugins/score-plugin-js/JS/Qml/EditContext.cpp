#include <JS/Qml/EditContext.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/File.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QFile>

namespace JS
{

const score::DocumentContext* EditJsContext::ctx()
{
  return score::GUIAppContext().currentDocument();
}

QObject* EditJsContext::metadata(QObject* obj) const noexcept
{
  if(!obj)
    return nullptr;
  return obj->findChild<score::ModelMetadata*>({}, Qt::FindDirectChildrenOnly);
}

void EditJsContext::undo()
{
  auto doc = ctx();
  if(!doc)
    return;
  doc->document.commandStack().undo();
}

void EditJsContext::redo()
{
  auto doc = ctx();
  if(!doc)
    return;
  doc->document.commandStack().redo();
}

QObject* EditJsContext::find(QString p)
{
  auto doc = document();
  const auto meta = doc->findChildren<score::ModelMetadata*>();
  for(auto m : meta)
  {
    if(m->getName() == p)
    {
      return m->parent();
    }
  }
  return nullptr;
}

QObject* EditJsContext::document()
{
  return score::GUIAppContext().documents.currentDocument();
}

QString EditJsContext::readFile(QString path)
{
  auto doc = ctx();
  if(!doc)
    return {};

  auto actual = score::locateFilePath(path, *doc);
  if(QFile f{actual}; f.exists() && f.open(QIODevice::ReadOnly))
  {
    return score::readFileAsQString(f);
  }
  else
  {
    return {};
  }
}

QObject* EditJsContext::selectedObject()
{
  auto doc = ctx();
  if(!doc)
    return {};

  const auto& cur = doc->selectionStack.currentSelection();
  if(cur.empty())
    return nullptr;

  return *cur.begin();
}

QVariantList EditJsContext::selectedObjects()
{
  auto doc = ctx();
  if(!doc)
    return {};

  const auto& cur = doc->selectionStack.currentSelection();
  if(cur.empty())
    return {};

  QVariantList list;
  for(auto& c : cur)
    list.push_back(QVariant::fromValue(c.data()));
  return list;
}

}
