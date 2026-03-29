#pragma once
#include <score_plugin_js_export.h>

class QWidget;
namespace Process
{
class ProcessModel;
}
namespace score
{
struct DocumentContext;
}

namespace JS
{

SCORE_PLUGIN_JS_EXPORT
QWidget* createPatcherWindow(
    Process::ProcessModel& proc, const score::DocumentContext& ctx,
    QWidget* parent = nullptr);

}
