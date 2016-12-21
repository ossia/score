#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_lib_base_export.h>

struct VisitorVariant;
class QObject;
namespace iscore
{
class DocumentDelegateModel;
class DocumentDelegatePresenter;
class DocumentDelegateView;
class DocumentModel;
class DocumentPresenter;
class DocumentView;
struct ApplicationContext;
struct DocumentContext;

/**
 * @brief Used to provide custom document types
 *
 * The interface required to create a custom main document (like MS Word's
 * main page)
 */
class ISCORE_LIB_BASE_EXPORT DocumentDelegateFactory
    : public iscore::Interface<DocumentDelegateFactory>
{
  ISCORE_INTERFACE("127ea824-f623-4f68-8deb-7c8c930a262b")
public:
  virtual ~DocumentDelegateFactory();

  virtual DocumentDelegateView*
  makeView(const iscore::ApplicationContext& ctx, QObject* parent)
      = 0;

  virtual DocumentDelegatePresenter* makePresenter(
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModel& model,
      DocumentDelegateView& view)
      = 0;

  virtual DocumentDelegateModel*
  make(const iscore::DocumentContext& ctx, DocumentModel* parent)
      = 0;
  virtual DocumentDelegateModel* load(
      const VisitorVariant&,
      const iscore::DocumentContext& ctx,
      DocumentModel* parent)
      = 0;
};

class ISCORE_LIB_BASE_EXPORT DocumentDelegateList final
    : public InterfaceList<iscore::DocumentDelegateFactory>
{
public:
  DocumentDelegateList()
  {
  }

  using object_type = DocumentDelegateModel;
};
}
