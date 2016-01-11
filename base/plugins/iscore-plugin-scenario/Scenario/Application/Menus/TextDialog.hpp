#pragma once

#include <QDialog>
#include <QString>
#include <iscore_plugin_scenario_export.h>

class QWidget;
namespace Scenario
{
// Has a big selectable text area.
class ISCORE_PLUGIN_SCENARIO_EXPORT TextDialog final : public QDialog
{
    public:
        TextDialog(const QString& s, QWidget* parent);
};
}
