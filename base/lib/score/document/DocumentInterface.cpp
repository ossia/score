// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentInterface.hpp"

#include <QObject>
#include <QString>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <stdexcept>

score::Document* score::IDocument::documentFromObject(const QObject* obj)
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

score::Document* score::IDocument::documentFromObject(const QObject& obj)
{
  return documentFromObject(&obj);
}

ObjectPath score::IDocument::unsafe_path(QObject const* const& obj)
{
  return ObjectPath::pathBetweenObjects(
      &documentFromObject(obj)->model().modelDelegate(), obj);
}

ObjectPath score::IDocument::unsafe_path(const QObject& obj)
{
  return unsafe_path(&obj);
}

score::DocumentDelegatePresenter*
score::IDocument::presenterDelegate_generic(const score::Document& d)
{
  if (d.presenter())
    return d.presenter()->presenterDelegate();
  return nullptr;
}

score::DocumentDelegateModel&
score::IDocument::modelDelegate_generic(const Document& d)
{
  return d.model().modelDelegate();
}

SCORE_LIB_BASE_EXPORT const score::DocumentContext&
score::IDocument::documentContext(const QObject& obj)
{
  auto doc = documentFromObject(obj);
  SCORE_ASSERT(doc);
  return doc->context();
}
