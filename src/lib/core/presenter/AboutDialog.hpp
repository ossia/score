#pragma once
#include <QDialog>

#include <score_lib_base_export.h>

namespace score
{
//! Help > About: the same content as the start screen's About page
class SCORE_LIB_BASE_EXPORT AboutDialog final : public QDialog
{
public:
  AboutDialog(QWidget* parent = nullptr);
};
}
