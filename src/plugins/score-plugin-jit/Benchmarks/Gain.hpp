#include <boost/align.hpp>

#include <vector>

using aligned_vec
    = std::vector<double, boost::alignment::aligned_allocator<double, 64>>;
void gain(
    const std::vector<aligned_vec>& p1,
    float g,
    std::vector<aligned_vec>& p2);
