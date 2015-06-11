#pragma once
#include <QComboBox>
#include <QList>
#include <QString>
#include "AddressSettings/AddressSettings.hpp"
inline void populateIOTypes(QComboBox* cbox)
{
    cbox->addItems(IOTypeStringMap().values());
}

inline void populateClipMode(QComboBox* cbox)
{
    cbox->addItems(ClipModeStringMap().values());
}
