#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/selection/SelectionStack.hpp>

class BaseElementPresenter;
class FullViewConstraintViewModel;
class ConstraintModel;
class ProcessSharedModelInterface;
class ProcessViewModelInterface;

/**
 * @brief The BaseElementModel class
 *
 * L'élément de base pour i-score est un constraintle,
 * qui ne contient qu'un seul Content. (le faire hériter pour cela ? )
 */

class BaseElementModel : public iscore::DocumentDelegateModelInterface
{
        Q_OBJECT

    public:
        BaseElementModel(const VisitorVariant& data, QObject* parent);

        BaseElementModel(QObject* parent);

        void initializeNewDocument(const FullViewConstraintViewModel* viewmodel);

        virtual ~BaseElementModel() = default;

        ConstraintModel* constraintModel() const
        { return m_baseConstraint; }

        ProcessViewModelInterface* focusedViewModel() const
        { return m_focusedViewModel; }

        ProcessSharedModelInterface* focusedProcess() const
        { return m_focusedProcess; }


        void serialize(const VisitorVariant&) const override;


        void setNewSelection(const Selection& s) override;
        void setDisplayedConstraint(ConstraintModel*);

    signals:
        void focusMe();
        void focusedViewModelChanged();

    public slots:
        void setFocusedViewModel(ProcessViewModelInterface* vm);

    private:
        ConstraintModel* m_baseConstraint {};
        ConstraintModel* m_displayedConstraint {};

        // The process that contains the current selection.
        ProcessViewModelInterface* m_focusedViewModel{};
        ProcessSharedModelInterface* m_focusedProcess{};
};

