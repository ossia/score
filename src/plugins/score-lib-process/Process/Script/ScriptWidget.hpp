#pragma once
#include <QWidget>

#include <score_lib_process_export.h>

#include <string_view>
class QTextEdit;
class QSyntaxStyle;
namespace Process
{
SCORE_LIB_PROCESS_EXPORT
QTextEdit* createScriptWidget(const std::string_view lang);

//! The editors' colour theme
SCORE_LIB_PROCESS_EXPORT
QSyntaxStyle* scriptStyle();

//! The same theme for an editor drawn over a preview: no background behind
//! the text and the line numbers, a barely visible current line.
SCORE_LIB_PROCESS_EXPORT
QSyntaxStyle* overlayScriptStyle();
}
