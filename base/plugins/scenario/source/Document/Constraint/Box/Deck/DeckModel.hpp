#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

class ProcessSharedModelInterface;
class ProcessViewModelInterface;
// TODO with composition instead of inheritance it would maybe be cleaner
// and allow us to use "traditional" copy ctors instead ?
class DeckModel : public IdentifiedObject<DeckModel>
{
        Q_OBJECT

        Q_PROPERTY(int height
                   READ height
                   WRITE setHeight
                   NOTIFY heightChanged)

    public:
        DeckModel(id_type<DeckModel> id, BoxModel* parent);

        // Copy
        DeckModel(DeckModel* source, id_type<DeckModel> id, BoxModel* parent);

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

        int height() const;
        // TODO put the position in the box.
        id_type<ProcessViewModelInterface> editedProcessViewModel() const
        {
            return m_editedProcessViewModelId;
        }

    signals:
        void processViewModelCreated(id_type<ProcessViewModelInterface> processViewModelId);
        void processViewModelRemoved(id_type<ProcessViewModelInterface> processViewModelId);
        void processViewModelSelected(id_type<ProcessViewModelInterface> processViewModelId);

        void heightChanged(int arg);

    public slots:
        void on_deleteSharedProcessModel(id_type<ProcessSharedModelInterface> sharedProcessId);

        void setHeight(int arg);

    private:
        id_type<ProcessViewModelInterface> m_editedProcessViewModelId {};
        std::vector<ProcessViewModelInterface*> m_processViewModels;

        int m_height {200};
};

/**
 * @brief parentConstraint Utility function to get the parent constraint of a process view model
 * @param pvm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(ProcessViewModelInterface* pvm);
