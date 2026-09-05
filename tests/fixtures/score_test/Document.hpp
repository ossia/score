#pragma once

// Document construction helpers for score tests.

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/Debug.hpp>

namespace score::test
{

/// Create a blank document using the first available document delegate.
/// In a normal build this is the Scenario document.
inline score::Document* new_document(const score::GUIApplicationContext& ctx)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());

  auto doc
      = ctx.docManager.newDocument(ctx, Id<score::DocumentModel>{}, *delegates.begin());

  QApplication::processEvents();
  QApplication::processEvents();
  return doc;
}

/// Serialize a document to the binary format and load it back as a new
/// document. Returns the reloaded document (owned by the DocumentManager).
inline score::Document*
reload_via_bytes(const score::GUIApplicationContext& ctx, score::Document& doc,
                 const QString& name = QStringLiteral("roundtrip"))
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());

  const QByteArray bytes = doc.saveAsByteArray();
  auto reloaded = ctx.docManager.loadDocument(
      ctx, name, bytes, DataStream::type(), *delegates.begin());

  QApplication::processEvents();
  QApplication::processEvents();
  return reloaded;
}

/// The .score (JSON) counterpart of saveAsJson: serialize a document exactly as
/// Document::save() writes a .score file, and load those bytes back. Kept
/// separate from reload_via_bytes because the two formats have INDEPENDENT
/// reader implementations -- a serialization defect fixed on one path can and
/// did survive on the other.
inline QByteArray save_as_json(score::Document& doc)
{
  JSONReader r;
  doc.saveAsJson(r);
  return QByteArray{r.buffer.GetString(), (int)r.buffer.GetSize()};
}

/// Serialize a document to JSON and load it back as a new document. Returns the
/// reloaded document (owned by the DocumentManager).
inline score::Document*
reload_via_json(const score::GUIApplicationContext& ctx, score::Document& doc,
                const QString& name = QStringLiteral("json-roundtrip"))
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());

  auto reloaded = ctx.docManager.loadDocument(
      ctx, name, save_as_json(doc), JSONObject::type(), *delegates.begin());

  QApplication::processEvents();
  QApplication::processEvents();
  return reloaded;
}

}
