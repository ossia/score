#pragma once
#include <tools/ObjectPath.hpp>

namespace iscore
{
	class Document;

	namespace IDocument
	{
		/**
	 * @brief documentFromObject
	 * @param obj an object in the application hierarchy
	 *
	 * @return the Document parent of the object or nullptr.
	 */
		Document* documentFromObject(QObject* obj);

		/**
	 * @brief pathFromDocument
	 * @param obj an object in the application hierarchy
	 *
	 * @return The path between a Document and this object.
	 */
		ObjectPath path(QObject* obj);
	}

}
