#pragma once
#include <QListWidget>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT AddRemoveList : public QListWidget
{
  W_OBJECT(AddRemoveList)
public:
  AddRemoveList(const QString& root, const QStringList& data, QWidget* parent);

  void on_itemChanged(QListWidgetItem* item);
  void fix(int k);

  void replaceContent(const QStringList& values);
  QStringList content() const noexcept;
  bool sameContent(const QStringList& values);

  void on_add(const QString& name);
  void on_remove();
  void setCount(int i);

  void changed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, changed)

  static void sanitize(AddRemoveList* changed, const AddRemoveList* other);

private:
  QString m_root;
  int m_editing = 0;
};
}
