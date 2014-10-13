#pragma once

#include <plugins_interface/capabilities/AbstractCapabilityFactory.h>


class Process
{
public:
		Process(int lol)
		{
			std::cerr << "lol: " << lol << std::endl;
		}
};

// Required template trick in order to have a generic make method for every process.
template<>
class AbstractCapabilityFactory<Process> : public AbstractCapabilityFactoryInterface
{
	public:
		virtual std::unique_ptr<Process> make(Args&&... args) = 0;
		// Ex.:  return std::make_unique<MyProcess>(std::forward<Args>(args)...);
		
	protected:
		using AbstractCapabilityFactoryInterface::AbstractCapabilityFactoryInterface;
};
	

class ProcessCapabilityFactory : public AbstractCapabilityFactory<Process>
{
	public:
		// Interface publique d'une factory de processus et objets associés
		// Pour afficher dans liste?
		std::string processName() { return m_processName; }
		
		// Si pas de vue n'afficher qu'une barre ?
		// Si une seule vue n'afficher qu'elle (en étendu / non-étendu)
		// Si plusieurs vues (mais pas les trois), priorité : vue médiane.
		// P-ê renvoyer un pointeur plus précis pour chaque type ?
		const AbstractCapabilityFactoryInterface_p& smallViewFactory();
		const AbstractCapabilityFactoryInterface_p& standardViewFactory();
		const AbstractCapabilityFactoryInterface_p& fullViewFactory();
		
		
		// Bien réfléchir au type des params : ptr? const? ref? rvalue-ref (&&)?
		// Est-ce que ça permet de faire du undo - redo ?
		// Est-ce que ça ne devrait pas aller sur la classe Process directement ? (si)
		
		// virtual std::unique_ptr<DragCommand> dragCommand(Process& process) = 0;
		// virtual std::unique_ptr<DropOnProcessCommand> dropCommand(Process& process, DropAction& action) = 0;
	

		ProcessCapabilityFactory(std::string&& process_name):
			AbstractCapabilityFactory<Process>("Process"),
			m_processName(std::move(process_name)) { }
			
	protected:
		AbstractCapabilityFactoryInterface_p m_smallView;
		AbstractCapabilityFactoryInterface_p m_stdView;
		AbstractCapabilityFactoryInterface_p m_fullView;

	private:
		std::string m_processName{};
};