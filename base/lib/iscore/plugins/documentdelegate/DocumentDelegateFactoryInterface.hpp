#pragma once
#include <iscore_lib_base_export.h>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

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
     * @brief The DocumentDelegateFactoryInterface class
     *
     * The interface required to create a custom main document (like MS Word's main page)
     */
class ISCORE_LIB_BASE_EXPORT DocumentDelegateFactory :
        public iscore::AbstractFactory<DocumentDelegateFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                DocumentDelegateFactory,
                "127ea824-f623-4f68-8deb-7c8c930a262b")
        public:
            virtual ~DocumentDelegateFactory();

        virtual DocumentDelegateViewInterface* makeView(
                const iscore::ApplicationContext& ctx,
                QObject* parent) = 0;

        virtual DocumentDelegatePresenterInterface* makePresenter(
                DocumentPresenter* parent_presenter,
                const DocumentDelegateModelInterface& model,
                DocumentDelegateViewInterface& view) = 0;

        virtual DocumentDelegateModelInterface* makeModel(
                const iscore::DocumentContext& ctx,
                DocumentModel* parent) = 0;
        virtual DocumentDelegateModelInterface* loadModel(
                const VisitorVariant&,
                const iscore::DocumentContext& ctx,
                DocumentModel* parent) = 0;
};

class ISCORE_LIB_BASE_EXPORT DocumentDelegateList final :
        public ConcreteFactoryList<iscore::DocumentDelegateFactory>
{
    public:
        using object_type = DocumentDelegateModelInterface;
};

}
