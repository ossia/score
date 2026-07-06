#pragma once

// Document construction helpers for score tests.

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
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

}
