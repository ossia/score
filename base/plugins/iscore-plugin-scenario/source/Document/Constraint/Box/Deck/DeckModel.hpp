#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

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
        void selectForEdition(
                const id_type<ProcessViewModel>& processViewId);
        const id_type<ProcessViewModel>& editedProcessViewModel() const;

        const std::vector<ProcessViewModel*>& processViewModels() const;
        ProcessViewModel& processViewModel(
                const id_type<ProcessViewModel>& processViewModelId) const;

        // A deck is always in a constraint
        ConstraintModel& parentConstraint() const;

        int height() const;
        bool focus() const;

    signals:
        void processViewModelCreated(const id_type<ProcessViewModel>& processViewModelId);
        void processViewModelRemoved(const id_type<ProcessViewModel>& processViewModelId);
        void processViewModelSelected(const id_type<ProcessViewModel>& processViewModelId);

        void heightChanged(int arg);
        void focusChanged(bool arg);

    public slots:
        void on_deleteSharedProcessModel(const id_type<ProcessModel>& sharedProcessId);

        void setHeight(int arg);
        void setFocus(bool arg);

    private:
        id_type<ProcessViewModel> m_editedProcessViewModelId {};
        std::vector<ProcessViewModel*> m_processViewModels;

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


/**
 * @brief identifierOfViewModelFromSharedModel
 * @param pvm A process view model
 *
 * @return A tuple which contains the required identifiers to get from a shared model to a given view model
 *  * The box identifier
 *  * The deck identifier
 *  * The view model identifier
 */
std::tuple<int, int, int> identifierOfProcessViewModelFromConstraint(ProcessViewModel* pvm);

QDataStream& operator<< (QDataStream& s, const std::tuple<int, int, int>& tuple);
QDataStream& operator>> (QDataStream& s, std::tuple<int, int, int>& tuple);

