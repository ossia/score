#pragma once

#include <string>
///////////
// Un plugin doit déclarer ses capacités.
// Ainsi que des handlers. Handler : sur une / plusieurs instances du même objet ?
// Pour simplifier on peut faire en sorte que les connections du handler se fassent en fonction des 
// capabilities des autres plug-ins
///////////

class AbstractCapabilityFactoryInterface
{
	public: 
		virtual ~AbstractCapabilityFactoryInterface() = default;
		std::string capabilityName() { return m_capabilityName; }
	
	protected:
		AbstractCapabilityFactoryInterface(std::string&& name):
			m_capabilityName{std::move(name)} { }
	private:
		std::string m_capabilityName{};
};

using AbstractCapabilityFactoryInterface_p = std::unique_ptr<AbstractCapabilityFactoryInterface>;
template<typename T>
class AbstractCapabilityFactory : public AbstractCapabilityFactoryInterface
{
	public: 
		// L'ownership du process va à la relation parente.
		// On utilise les templates pour pouvoir forward tranquillement les arguments
		// au constructeur de Process, s'il y en a besoin.
		// Pourquoi ne pas généraliser la méthode make dans ce cas?
		template<typename... Args>
		std::unique_ptr<T> make(Args&&...);
		
	protected:
		using AbstractCapabilityFactoryInterface::AbstractCapabilityFactoryInterface;
};