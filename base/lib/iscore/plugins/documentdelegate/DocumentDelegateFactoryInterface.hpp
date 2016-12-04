#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_lib_base_export.h>

struct VisitorVariant;
class QObject;
namespace iscore
{
class DocumentDelegateModelInterface;
class DocumentDelegatePresenterInterface;
class DocumentDelegateViewInterface;
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
    : public iscore::AbstractFactory<DocumentDelegateFactory>
{
  ISCORE_ABSTRACT_FACTORY("127ea824-f623-4f68-8deb-7c8c930a262b")
public:
  virtual ~DocumentDelegateFactory();

  virtual DocumentDelegateViewInterface*
  makeView(const iscore::ApplicationContext& ctx, QObject* parent)
      = 0;

  virtual DocumentDelegatePresenterInterface* makePresenter(
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModelInterface& model,
      DocumentDelegateViewInterface& view)
      = 0;

  virtual DocumentDelegateModelInterface*
  make(const iscore::DocumentContext& ctx, DocumentModel* parent)
      = 0;
  virtual DocumentDelegateModelInterface* load(
      const VisitorVariant&,
      const iscore::DocumentContext& ctx,
      DocumentModel* parent)
      = 0;
};

class ISCORE_LIB_BASE_EXPORT DocumentDelegateList final
    : public ConcreteFactoryList<iscore::DocumentDelegateFactory>
{
public:
  DocumentDelegateList()
  {
  }

  using object_type = DocumentDelegateModelInterface;
};
}
