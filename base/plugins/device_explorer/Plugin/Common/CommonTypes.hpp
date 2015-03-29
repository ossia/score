#pragma once

inline QList<QString> getIOTypes()
{
    return {QObject::tr("In"), QObject::tr("Out"), QObject::tr("In/Out")};
}

inline QList<QString> getUnits()
{
    return {
    QObject::tr("--"),
    QObject::tr("Hz"),
    QObject::tr("dB"),
    QObject::tr("s"),
    QObject::tr("ms"),
    QObject::tr("m"),
    QObject::tr("mm"),
    };
}

inline QList<QString> getClipModes()
{
    return {
    QObject::tr("none"),
    QObject::tr("low"),
    QObject::tr("high"),
    QObject::tr("both"),
    };
}

#include <QComboBox>

inline void populateIOTypes(QComboBox* cbox)
{
    Q_ASSERT(cbox);
    cbox->addItems(getIOTypes());
}

inline void populateUnit(QComboBox* cbox)
{
    Q_ASSERT(cbox);
    cbox->addItems(getUnits());
}

inline void populateClipMode(QComboBox* cbox)
{
    Q_ASSERT(cbox);
    cbox->addItems(getClipModes());
}
