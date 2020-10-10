#pragma once
#include <QWidget>
#include <string_view>
#include <score_lib_process_export.h>
class QCodeEditor;
namespace Process
{
SCORE_LIB_PROCESS_EXPORT
QCodeEditor* createScriptWidget(const std::string_view lang);
}
