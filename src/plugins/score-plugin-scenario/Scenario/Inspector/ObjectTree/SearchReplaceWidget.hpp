#pragma once
#include <QDialog>
#include <QContextMenuEvent>
#include <vector>
#include <State/Expression.hpp>
namespace Scenario{
class SearchReplaceWidget: public QDialog
{
public:
    SearchReplaceWidget(const score::GUIApplicationContext& ctx);
    void search();
    void replace();
    void setFindTarget(const QString& fTarget);
    void setReplaceTarget(const QString& rTarget);
private:
    void replaceAddress(State::Expression& expr,
                        const State::Address& oldAddr,
                        const State::Address& newAddr);
    QString getObjectName(const QObject* o);
    const score::GUIApplicationContext& m_ctx;
    State::Address m_oldAddress{};
    State::Address m_newAddress{};
    std::vector<QObject*> m_matches{};

};

}
