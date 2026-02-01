#include <JS/Qml/EditContext.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
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

  auto& stack = doc->document.commandStack();
  if(stack.canUndo())
    stack.undo();
}

void EditJsContext::redo()
{
  auto doc = ctx();
  if(!doc)
    return;

  auto& stack = doc->document.commandStack();
  if(stack.canRedo())
    stack.redo();
}

QObject* EditJsContext::find(QString p)
{
  auto doc = document();
  if(!doc)
    return nullptr;

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

QObject* EditJsContext::findByLabel(QString p)
{
  auto doc = document();
  const auto meta = doc->findChildren<score::ModelMetadata*>();
  for(auto m : meta)
  {
    if(m->getLabel() == p)
    {
      return m->parent();
    }
  }
  return nullptr;
}

void EditJsContext::load(QString doc)
{
  auto& documents = score::GUIAppContext().docManager;
  documents.loadFile(score::GUIAppContext(), doc);
}

void EditJsContext::save()
{
  auto doc = (score::Document*)document();
  if(!doc)
    return;

  auto& documents = score::GUIAppContext().docManager;
  documents.saveDocument(*doc);
}

void EditJsContext::saveAs(QString path)
{
  auto doc = (score::Document*)document();
  if(!doc)
    return;

  auto& documents = score::GUIAppContext().docManager;
  documents.saveDocumentAs(*doc, path);
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

QObject* EditJsContext::documentPlugin(QString key)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;

  auto uuid = key.toLatin1();
  if(uuid.length() != 36)
    return nullptr;
  auto uid = score::uuids::string_generator::compute(uuid.begin(), uuid.end());
  if(uid.is_nil())
    return nullptr;
  auto k = UuidKey<score::DocumentPluginFactory>{uid};
  for(auto* plug : doc->pluginModels())
  {
    if(auto p = qobject_cast<score::SerializableDocumentPlugin*>(plug))
    {
      if(p->concreteKey() == k)
      {
        return p;
      }
    }
  }
  return nullptr;
}
}
