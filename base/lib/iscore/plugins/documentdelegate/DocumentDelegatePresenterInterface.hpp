#pragma once
#include <QObject>
#include <QString>
#include <iscore_lib_base_export.h>

namespace iscore
{
class DocumentDelegateModelInterface;
class DocumentDelegateViewInterface;
class DocumentPresenter;

class ISCORE_LIB_BASE_EXPORT DocumentDelegatePresenterInterface :
        public QObject
{
    public:
        DocumentDelegatePresenterInterface(DocumentPresenter* parent_presenter,
                                           const DocumentDelegateModelInterface& model,
                                           DocumentDelegateViewInterface& view);

        virtual ~DocumentDelegatePresenterInterface();

    protected:
        const DocumentDelegateModelInterface& m_model;
        DocumentDelegateViewInterface& m_view;
        DocumentPresenter* m_parentPresenter{};
};
}
