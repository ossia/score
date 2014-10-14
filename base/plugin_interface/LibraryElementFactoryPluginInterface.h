#pragma once
#include <interface/library/LibraryElement.h>

namespace iscore
{
	// Families of elements that can be added to the library, and how to react to the drag / drop / load... operations offered 
	// by elements of the plug-in.
	class LibraryElementFactoryPluginInterface 
	{
		virtual ~LibraryElementFactoryPluginInterface() = default;
		
        virtual QStringList libraryElement_list() const = 0;
		virtual std::unique_ptr<LibraryElement> libraryElement_make(QString) = 0;
	};
}
