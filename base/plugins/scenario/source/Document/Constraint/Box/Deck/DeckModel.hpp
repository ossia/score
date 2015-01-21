#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

class ProcessViewModelInterface;
class DeckModel : public IdentifiedObjectAlternative<DeckModel>
{
	Q_OBJECT

		Q_PROPERTY(int height
				   READ height
				   WRITE setHeight
				   NOTIFY heightChanged)

	public:
		DeckModel(int position, id_type<DeckModel> id, BoxModel* parent);

		template<typename Impl>
		DeckModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObjectAlternative<DeckModel>{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual ~DeckModel() = default;

		void createProcessViewModel(int sharedProcessId, int newProcessViewModelId);
		void addProcessViewModel(ProcessViewModelInterface*);
		void deleteProcessViewModel(int processViewModelId);

		/**
		 * @brief selectForEdition
		 * @param processViewId
		 *
		 * A process is selected for edition when it is
		 * the edited process when the interface is clicked.
		 */
		void selectForEdition(int processViewId);

		const std::vector<ProcessViewModelInterface*>& processViewModels() const;
		ProcessViewModelInterface* processViewModel(int processViewModelId) const;

		/**
		 * @brief parentConstraint
		 * @return the constraint this deck is part of.
		 */
		ConstraintModel* parentConstraint() const;

		int height() const;
		int position() const;
		int editedProcessViewModel() const
		{ return m_editedProcessViewModelId; }

	signals:
		void processViewModelCreated(int processViewModelId);
		void processViewModelRemoved(int processViewModelId);
		void processViewModelSelected(int processViewModelId);

		void heightChanged(int arg);
		void positionChanged(int arg);

	public slots:
		void on_deleteSharedProcessModel(int sharedProcessId);

		void setHeight(int arg);
		void setPosition(int arg);

	private:
		int m_editedProcessViewModelId{};
		std::vector<ProcessViewModelInterface*> m_processViewModels;

		int m_height{200};
		int m_position{};
};

/**
 * @brief parentConstraint Utility function to get the parent constraint of a process view model
 * @param pvm Process view model pointer
 *
 * @return A pointer to the parent constraint if there is one, or nullptr.
 */
ConstraintModel* parentConstraint(ProcessViewModelInterface* pvm);
