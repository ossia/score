// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QObject>
#include <QString>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <stdexcept>

#include "DocumentInterface.hpp"
#include <iscore/model/path/ObjectPath.hpp>

iscore::Document* iscore::IDocument::documentFromObject(const QObject* obj)
{
  QString objName{obj ? obj->objectName() : "INVALID"};

  while (obj && !qobject_cast<const Document*>(obj))
  {
    obj = obj->parent();
  }

  if (!obj)
  {
    qDebug("fail");
    throw std::runtime_error(
        QStringLiteral("Object (name: %1) is not part of a Document!")
            .arg(objName)
            .toStdString());
  }

  return safe_cast<Document*>(const_cast<QObject*>(obj));
}

iscore::Document* iscore::IDocument::documentFromObject(const QObject& obj)
{
  return documentFromObject(&obj);
}

ObjectPath iscore::IDocument::unsafe_path(QObject const* const& obj)
{
  return ObjectPath::pathBetweenObjects(
      &documentFromObject(obj)->model(), obj);
}

ObjectPath iscore::IDocument::unsafe_path(const QObject& obj)
{
  return unsafe_path(&obj);
}

iscore::DocumentDelegatePresenter&
iscore::IDocument::presenterDelegate_generic(const iscore::Document& d)
{
  return d.presenter().presenterDelegate();
}

iscore::DocumentDelegateModel&
iscore::IDocument::modelDelegate_generic(const Document& d)
{
  return d.model().modelDelegate();
}

ISCORE_LIB_BASE_EXPORT const iscore::DocumentContext&
iscore::IDocument::documentContext(const QObject& obj)
{
  auto doc = documentFromObject(obj);
  ISCORE_ASSERT(doc);
  return doc->context();
}
