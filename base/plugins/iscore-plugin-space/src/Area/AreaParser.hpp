#pragma once
#include <Space/area.hpp>
#include <QStringList>

namespace Space
{
class AreaParser
{
    public:
        AreaParser(const QStringList& str);

        bool check() const;
        std::unique_ptr<spacelib::area> result();


    private:

        std::vector<
            std::pair<
                QStringList,
                GiNaC::relational::operators
            >
        > m_parsed;

};

// TODO AreaValidator for QLineEdit..
}
