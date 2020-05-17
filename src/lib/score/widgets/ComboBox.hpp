#pragma once

#include <QComboBox>

#include <score_lib_base_export.h>

namespace score {
class SCORE_LIB_BASE_EXPORT ComboBox : public QComboBox
{
public:
    ComboBox (QWidget* parent): QComboBox(parent)
    {
      setDisabled(true);
      connect(this, qOverload<int>(&QComboBox::currentIndexChanged),
              this, [&](int index){this->setEnabled(index != -1);});
    }
};
}
