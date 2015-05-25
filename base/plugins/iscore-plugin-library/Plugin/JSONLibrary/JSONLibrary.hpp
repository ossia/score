#pragma once
#include <QVector>
#include "LibraryElement.hpp"

class JSONLibrary
{
    public:
        QVector<LibraryElement> elements;

        void addElement(const LibraryElement& e);
};
