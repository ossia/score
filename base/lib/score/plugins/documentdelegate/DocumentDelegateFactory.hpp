#pragma once
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score_lib_base_export.h>

struct VisitorVariant;
class QObject;
namespace score
{
class DocumentDelegateModel;
class DocumentDelegatePresenter;
class DocumentDelegateView;
class DocumentModel;
class DocumentPresenter;
class DocumentView;
struct GUIApplicationContext;
struct DocumentContext;

/**
 * @brief Used to provide custom document types
 *
 * The interface required to create a custom main document (like MS Word's
 * main page)
 */
class SCORE_LIB_BASE_EXPORT DocumentDelegateFactory
    : public score::InterfaceBase
{
  SCORE_INTERFACE(DocumentDelegateFactory, "127ea824-f623-4f68-8deb-7c8c930a262b")
public:
  ~DocumentDelegateFactory() override;

  virtual DocumentDelegateView*
  makeView(const score::DocumentContext& ctx, QObject* parent)
      = 0;

  virtual DocumentDelegatePresenter* makePresenter(
      const score::DocumentContext& ctx,
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModel& model,
      DocumentDelegateView& view)
      = 0;

  virtual void make(
      const score::DocumentContext& ctx,
      DocumentDelegateModel*& ptr,
      DocumentModel* parent)
      = 0;
  virtual void load(
      const VisitorVariant&,
      const score::DocumentContext& ctx,
      DocumentDelegateModel*& ptr,
      DocumentModel* parent)
      = 0;
};

class SCORE_LIB_BASE_EXPORT DocumentDelegateList final
    : public InterfaceList<score::DocumentDelegateFactory>
{
public:
  ~DocumentDelegateList() override;

  using object_type = DocumentDelegateModel;
};
}
