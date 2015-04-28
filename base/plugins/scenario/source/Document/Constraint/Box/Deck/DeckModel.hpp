#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

class ProcessSharedModelInterface;
class ProcessViewModelInterface;

// Note : the DeckModel is assumed to be in a Box, itself in a Constraint.
class DeckModel : public IdentifiedObject<DeckModel>
{
        Q_OBJECT

        Q_PROPERTY(int height
                   READ height
                   WRITE setHeight
                   NOTIFY heightChanged)

        Q_PROPERTY(bool focus
                   READ focus
                   WRITE setFocus
                   NOTIFY focusChanged)

    public:
        DeckModel(id_type<DeckModel> id,
                  BoxModel* parent);

        // Copy
        DeckModel(std::function<void(DeckModel&, DeckModel&)> pvmCopyMethod,
                  DeckModel* source,
                  id_type<DeckModel> id,
                  BoxModel* parent);

        static void copyViewModelsInSameConstraint(DeckModel&, DeckModel&);

        template<typename Impl>
        DeckModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<DeckModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual ~DeckModel() = default;

        void addProcessViewModel(ProcessViewModelInterface*);
        void deleteProcessViewModel(id_type<ProcessViewModelInterface> processViewModelId);

        /**
         * @brief selectForEdition
         * @param processViewId
         *
         * A process is selected for edition when it is
         * the edited process when the interface is clicked.
         */
        void selectForEdition(id_type<ProcessViewModelInterface> processViewId);

        const std::vector<ProcessViewModelInterface*>& processViewModels() const;
        ProcessViewModelInterface* processViewModel(id_type<ProcessViewModelInterface> processViewModelId) const;

        /**
         * @brief parentConstraint
         * @return the constraint this deck is part of.
         */
        ConstraintModel* parentConstraint() const;


        id_type<ProcessViewModelInterface> editedProcessViewModel() const
        {
            return m_editedProcessViewModelId;
        }

        int height() const;
        bool focus() const;

    signals:
        void processViewModelCreated(id_type<ProcessViewModelInterface> processViewModelId);
        void processViewModelRemoved(id_type<ProcessViewModelInterface> processViewModelId);
        void processViewModelSelected(id_type<ProcessViewModelInterface> processViewModelId);

        void heightChanged(int arg);
        void focusChanged(bool arg);

    public slots:
        void on_deleteSharedProcessModel(id_type<ProcessSharedModelInterface> sharedProcessId);

        void setHeight(int arg);
        void setFocus(bool arg);

    private:
        id_type<ProcessViewModelInterface> m_editedProcessViewModelId {};
        std::vector<ProcessViewModelInterface*> m_processViewModels;

        int m_height {200};
        bool m_focus{false};
};

/**
 * @brief parentConstraint Utility function to get the parent constraint of a process view model
 * @param pvm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(ProcessViewModelInterface* pvm);
