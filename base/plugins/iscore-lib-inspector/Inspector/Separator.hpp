#pragma once
#include <QWidget>
#include <iscore_lib_base_export.h>


namespace Inspector
{
class ISCORE_LIB_BASE_EXPORT HSeparator : public QWidget
{
    public:
        explicit HSeparator(QWidget* parent);
        ~HSeparator();
};

class ISCORE_LIB_BASE_EXPORT VSeparator : public QWidget
{
    public:
        explicit VSeparator(QWidget* parent);
        ~VSeparator();
};
}
