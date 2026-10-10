#pragma once
#include <QString>

#include <score_lib_base_export.h>
class QWidget;
namespace score
{

//! Whether a human is there to answer a dialog: false in tests
//! (SCORE_SANITIZE_SKIP_CHECKS), without a GUI, when a script drives the
//! session, and on the offscreen and minimal platforms.
SCORE_LIB_BASE_EXPORT bool interactiveSession() noexcept;

SCORE_LIB_BASE_EXPORT int
question(QWidget* parent, const QString& title, const QString& text);

SCORE_LIB_BASE_EXPORT int
information(QWidget* parent, const QString& title, const QString& text);

SCORE_LIB_BASE_EXPORT int
warning(QWidget* parent, const QString& title, const QString& text);

}
