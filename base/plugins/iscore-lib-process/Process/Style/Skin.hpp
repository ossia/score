#pragma once
#include <QObject>
#include <boost/bimap.hpp>
#include <iscore_lib_process_export.h>
#include <QFont>
#include <QColor>
#include <QVector>
#include <QPair>
class ISCORE_LIB_PROCESS_EXPORT Skin :
        public QObject
{
        Q_OBJECT
    public:
        static Skin& instance();

        void load(const QJsonObject& style);

        QFont SansFont;
        QFont MonoFont;

        QColor Dark;
        QColor HalfDark;
        QColor Gray;
        QColor HalfLight;
        QColor Light;

        QColor Emphasis1;
        QColor Emphasis2;
        QColor Emphasis3;
        QColor Emphasis4;

        QColor Base1;
        QColor Base2;
        QColor Base3;
        QColor Base4;

        QColor Warn1;
        QColor Warn2;
        QColor Warn3;

        QColor Background1;

        QColor Transparent1;
        QColor Transparent2;
        QColor Transparent3;

        QColor Smooth1;
        QColor Smooth2;
        QColor Smooth3;

        QColor Tender1;
        QColor Tender2;
        QColor Tender3;

        const QColor* fromString(const QString& s) const;
        QString toString(const QColor*) const;

        QVector<QPair<QColor,QString>> getColors() const;

    signals:
        void changed();

    private:
        Skin() noexcept;

        boost::bimap<QString, const QColor*> m_colorMap;

};
