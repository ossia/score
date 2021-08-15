#pragma once
#include <QWidget>

#include <score_lib_process_export.h>

#include <string_view>
class QTextEdit;
namespace Process
{
SCORE_LIB_PROCESS_EXPORT
QTextEdit* createScriptWidget(const std::string_view lang);
}
