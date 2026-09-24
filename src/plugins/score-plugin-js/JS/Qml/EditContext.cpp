#include <score/model/path/ObjectPath.hpp>
#include <JS/Qml/EditContext.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/File.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QFile>
#include <QQmlEngine>

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

QObject* EditJsContext::documentMetadata() const noexcept
{
  auto doc = score::GUIAppContext().currentDocument();
  if(!doc)
    return nullptr;

  auto& meta = doc->document.metadata();
  // It is a member of the Document and has no QObject parent, so the engine
  // would take it for its own and delete it: the document then frees it a
  // second time on the way out.
  QQmlEngine::setObjectOwnership(&meta, QQmlEngine::CppOwnership);
  return &meta;
}

QString EditJsContext::documentName() const noexcept
{
  auto doc = score::GUIAppContext().currentDocument();
  if(!doc)
    return {};
  return doc->document.metadata().documentName();
}

void EditJsContext::setDocumentName(QString name)
{
  auto doc = score::GUIAppContext().currentDocument();
  if(!doc)
    return;
  doc->document.metadata().setFileName(std::move(name));
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

bool EditJsContext::canUndo()
{
  auto doc = ctx();
  return doc ? doc->document.commandStack().canUndo() : false;
}

bool EditJsContext::canRedo()
{
  auto doc = ctx();
  return doc ? doc->document.commandStack().canRedo() : false;
}

int EditJsContext::undoIndex()
{
  auto doc = ctx();
  return doc ? doc->document.commandStack().currentIndex() : 0;
}

int EditJsContext::undoCount()
{
  auto doc = ctx();
  return doc ? doc->document.commandStack().size() : 0;
}

QString EditJsContext::undoText()
{
  auto doc = ctx();
  if(!doc)
    return {};
  // CommandStack returns a localised placeholder on an empty stack.
  auto& stack = doc->document.commandStack();
  return stack.canUndo() ? stack.undoText() : QString{};
}

QString EditJsContext::redoText()
{
  auto doc = ctx();
  if(!doc)
    return {};
  auto& stack = doc->document.commandStack();
  return stack.canRedo() ? stack.redoText() : QString{};
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

QString EditJsContext::path(QObject* obj)
{
  auto doc = ctx();
  if(!doc || !obj)
    return {};
  try
  {
    auto full = ObjectPath::pathBetweenObjects(&doc->document.model(), obj);
    auto& v = full.vec();
    return ObjectPath{{v.begin() + 1, v.end()}}.toString();
  }
  catch(...)
  {
    return {};
  }
}

QObject* EditJsContext::findByPath(QString path)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  return ObjectPath::fromString(path).findObject(*doc);
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

QString EditJsContext::relativizeFilePath(QString path)
{
  auto doc = ctx();
  if(!doc)
    return path;

  return score::relativizeFilePath(path, *doc);
}

QString EditJsContext::locateFilePath(QString path)
{
  auto doc = ctx();
  if(!doc)
    return path;

  return score::locateFilePath(path, *doc);
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

void EditJsContext::select(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return;

  auto identified = qobject_cast<IdentifiedObjectAbstract*>(obj);
  if(!identified)
    doc->selectionStack.deselect();
  else
    doc->selectionStack.pushNewSelection(Selection{identified});
}

void EditJsContext::select(QVariantList objs)
{
  auto doc = ctx();
  if(!doc)
    return;

  Selection sel;
  for(const auto& v : objs)
    if(auto obj = qobject_cast<IdentifiedObjectAbstract*>(v.value<QObject*>()))
      sel.append(obj);

  doc->selectionStack.pushNewSelection(sel);
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
