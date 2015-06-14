#pragma once
#include <ProcessInterface/ProcessViewModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

class ProcessModel;
class ProcessViewModel;

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
        DeckModel(const id_type<DeckModel>& id,
                  BoxModel* parent);

        // Copy
        DeckModel(std::function<void(const DeckModel&, DeckModel&)> pvmCopyMethod,
                  const DeckModel& source,
                  const id_type<DeckModel>& id,
                  BoxModel* parent);

        static void copyViewModelsInSameConstraint(const DeckModel&, DeckModel&);

        template<typename Impl>
        DeckModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<DeckModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual ~DeckModel() = default;

        void addProcessViewModel(
                ProcessViewModel*);
        void deleteProcessViewModel(
                const id_type<ProcessViewModel>& processViewModelId);


         // A process is selected for edition when it is
         // the edited process when the interface is clicked.
        void putToFront(
                const id_type<ProcessViewModel>& processViewId);
        const id_type<ProcessViewModel>& frontProcessViewModel() const;

        const auto& processViewModels() const
        { return m_processViewModels; }

        ProcessViewModel& processViewModel(
                const id_type<ProcessViewModel>& processViewModelId) const;

        // A deck is always in a constraint
        ConstraintModel& parentConstraint() const;

        int height() const;
        bool focus() const;

    signals:
        void processViewModelCreated(const id_type<ProcessViewModel>& processViewModelId);
        void processViewModelRemoved(const id_type<ProcessViewModel>& processViewModelId);
        void processViewModelPutToFront(const id_type<ProcessViewModel>& processViewModelId);

        void heightChanged(int arg);
        void focusChanged(bool arg);

    public slots:
        void on_deleteSharedProcessModel(const id_type<ProcessModel>& sharedProcessId);

        void setHeight(int arg);
        void setFocus(bool arg);

    private:
        id_type<ProcessViewModel> m_frontProcessViewModelId;
        IdContainer<ProcessViewModel> m_processViewModels;

        int m_height {200};
        bool m_focus{false};
};

/**
 * @brief parentConstraint Utility function to get the parent constraint of a process view model
 * @param pvm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(ProcessViewModel* pvm);
