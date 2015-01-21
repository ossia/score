#pragma once
#include <tools/IdentifiedObject.hpp>

#include <vector>

class BoxModel;
class ConstraintModel;

class ProcessSharedModelInterface;
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

		void createProcessViewModel(id_type<ProcessSharedModelInterface> sharedProcessId,
									id_type<ProcessViewModelInterface> newProcessViewModelId);
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
		int position() const;
		id_type<ProcessViewModelInterface> editedProcessViewModel() const
		{ return m_editedProcessViewModelId; }

	signals:
		void processViewModelCreated(id_type<ProcessViewModelInterface> processViewModelId);
		void processViewModelRemoved(id_type<ProcessViewModelInterface> processViewModelId);
		void processViewModelSelected(id_type<ProcessViewModelInterface> processViewModelId);

		void heightChanged(int arg);
		void positionChanged(int arg);

	public slots:
		void on_deleteSharedProcessModel(id_type<ProcessSharedModelInterface> sharedProcessId);

		void setHeight(int arg);
		void setPosition(int arg);

	private:
		id_type<ProcessViewModelInterface> m_editedProcessViewModelId{};
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
