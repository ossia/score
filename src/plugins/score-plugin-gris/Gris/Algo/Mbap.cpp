/**
 * Matrix-Based Amplitude Panning framework.
 *
 * MBAP (Matrix-Based Amplitude Panning) is a framework
 * to do 3-D sound spatialization. It uses a pre-computed
 * gain matrix to perform the spatialization of the sources very
 * efficiently.
 *
 * author : Gaël Lane Lépine, 2022
 * based on lbap from Olivier Belanger
 *
 */

/*
 This file is part of SpatGRIS.

 Developers: Gaël Lane Lépine, Samuel Béland, Olivier Bélanger, Nicolas Masson

 SpatGRIS is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SpatGRIS is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SpatGRIS.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Gris/Algo/Mbap.hpp>

#include <Gris/Algo/SpeakerSetup.hpp>



#include <cassert>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

namespace Gris
{
namespace
{
//==============================================================================
/* Trilinear interpolation to retrieve the value at position (x, y, z) in a 3D matrix. */
static float trilinearInterpolation(matrix_t const & matrix, float const x, float const y, float const z)
{
    assert(x >= 0.0f && y >= 0.0f && z >= 0.0f);
    auto const xi = static_cast<std::size_t>(x);
    auto const yi = static_cast<std::size_t>(y);
    auto const zi = static_cast<std::size_t>(z);
    auto const xf = narrow<float>(x) - narrow<float>(xi);
    auto const yf = narrow<float>(y) - narrow<float>(yi);
    auto const zf = narrow<float>(z) - narrow<float>(zi);
    auto const v1 = matrix[xi][yi][zi];
    auto const v2 = matrix[xi + 1][yi][zi];
    auto const v3 = matrix[xi][yi + 1][zi];
    auto const v4 = matrix[xi + 1][yi + 1][zi];
    auto const v5 = matrix[xi][yi][zi + 1];
    auto const v6 = matrix[xi + 1][yi][zi + 1];
    auto const v7 = matrix[xi][yi + 1][zi + 1];
    auto const v8 = matrix[xi + 1][yi + 1][zi + 1];

    // from
    // https://www.scratchapixel.com/code.php?id=56&origin=/lessons/mathematics-physics-for-computer-graphics/interpolation
    return (1 - xf) * (1 - yf) * (1 - zf) * v1 + xf * (1 - yf) * (1 - zf) * v2 + (1 - xf) * yf * (1 - zf) * v3
           + xf * yf * (1 - zf) * v4 + (1 - xf) * (1 - yf) * zf * v5 + xf * (1 - yf) * zf * v6 + (1 - xf) * yf * zf * v7
           + xf * yf * zf * v8;
}

//==============================================================================
/* Returns a vector mbap_pos created from an array of mbap_speaker.
 */
static std::vector<Position> mbapPositionsFromSpeakers(MbapSpeaker const * speakers, size_t const num)
{
    std::vector<Position> positions{};
    positions.reserve(num);
    std::transform(speakers, speakers + num, std::back_inserter(positions), [](MbapSpeaker const & speaker) {
        return speaker.position;
    });
    return positions;
}

//==============================================================================
/* Initialize a newly created field for `num` speakers. */
static MbapField initField(std::vector<Position> speakers)
{
    MbapField field{};

    field.amplitudeMatrix.reserve(speakers.size());
    static constexpr matrix_t EMPTY_MATRIX{};
    std::fill_n(std::back_inserter(field.amplitudeMatrix), speakers.size(), EMPTY_MATRIX);
    field.speakerPositions = std::move(speakers);

    return field;
}

//==============================================================================
/* Pre-compute the 3 dimensional matrix of amplitude for the speakers. */
static void computeMatrix(MbapField & field)
{
    static auto constexpr H_SIZE = MBAP_MATRIX_SIZE / 2;

    for (size_t i{}; i < field.speakerPositions.size(); ++i) {
        auto const px = field.speakerPositions[i].getCartesian().x * H_SIZE + H_SIZE;
        auto const py = field.speakerPositions[i].getCartesian().y * H_SIZE + H_SIZE;
        auto const pz = field.speakerPositions[i].getCartesian().z * H_SIZE + H_SIZE;

        for (size_t x{}; x < MBAP_MATRIX_SIZE; ++x) {
            for (size_t y{}; y < MBAP_MATRIX_SIZE; ++y) {
                for (size_t z{}; z < MBAP_MATRIX_SIZE; ++z) {
                    auto dist = std::sqrt(std::pow(narrow<float>(x) - px, 2.0f) + std::pow(narrow<float>(y) - py, 2.0f)
                                          + std::pow(narrow<float>(z) - pz, 2.0f));

                    dist = std::pow(std::pow(10.0f, 1.0f / 20), dist);          // root-power ratio
                    field.amplitudeMatrix[i][x][y][z] = 1.0f / std::sqrt(dist); // inverse square law
                }
                field.amplitudeMatrix[i][x][y][MBAP_MATRIX_SIZE] = field.amplitudeMatrix[i][x][y][0];
            }
            field.amplitudeMatrix[i][x][MBAP_MATRIX_SIZE] = field.amplitudeMatrix[i][x][0];
        }
        field.amplitudeMatrix[i][MBAP_MATRIX_SIZE] = field.amplitudeMatrix[i][0];
    }
}

//==============================================================================
/* Create the field */
static MbapField createField(std::vector<Position> speakers)
{
    auto result{ initField(std::move(speakers)) };
    computeMatrix(result);
    return result;
}

//==============================================================================
/* Compute the gain of field of speakers, for the given position, and store the result in the `gains` array.*/
static void computeGains(MbapField const & field, SourceData const & source, float * gains)
{
    static constexpr auto H_SIZE = MBAP_MATRIX_SIZE / 2.0f;
    static constexpr auto SIZE_MINUS_ONE = MBAP_MATRIX_SIZE - 1.0f;

    auto constexpr EXPONENT_MIN_IN{ 0.0f };
    auto constexpr EXPONENT_MAX_IN{ 1.0f };
    auto constexpr EXPONENT_MIN_OUT{ 4.0f };
    auto constexpr EXPONENT_MAX_OUT{ 8.0f };

    assert(source.position);

    auto x = source.position->getCartesian().x * (H_SIZE - 1.0f) + H_SIZE;
    auto y = source.position->getCartesian().y * (H_SIZE - 1.0f) + H_SIZE;
    auto z = source.position->getCartesian().z * (H_SIZE - 1.0f) + H_SIZE;
    x = std::clamp(x, 0.0f, SIZE_MINUS_ONE);
    y = std::clamp(y, 0.0f, SIZE_MINUS_ONE);
    z = std::clamp(z, 0.0f, SIZE_MINUS_ONE);

    auto const sourceAzimuthSpan{ source.azimuthSpan };
    auto const sourceElevationSpan{ source.zenithSpan };
    auto const sumAziElevSpans{ 1.0f - sourceAzimuthSpan + 1.0f - sourceElevationSpan };

    float distFromSource{};
    float distXYPlane{};
    float distZ{};

    assert(field.speakerPositions.size() == field.amplitudeMatrix.size());

    auto const finalElevSpanExponent{ ((sourceElevationSpan - EXPONENT_MIN_IN) * (EXPONENT_MAX_OUT - EXPONENT_MIN_OUT)
                                       / (EXPONENT_MAX_IN - EXPONENT_MIN_IN))
                                      + EXPONENT_MIN_OUT };

    auto const finalAzimuthSpanExponent{ ((sourceAzimuthSpan - EXPONENT_MIN_IN) * (EXPONENT_MAX_OUT - EXPONENT_MIN_OUT)
                                          / (EXPONENT_MAX_IN - EXPONENT_MIN_IN))
                                         + EXPONENT_MIN_OUT };

    for (size_t i{}; i < static_cast<size_t>(field.speakerPositions.size()); ++i) {
        auto squaredDistX = field.speakerPositions[i].getCartesian().x - source.position->getCartesian().x;
        squaredDistX *= squaredDistX;
        auto squaredDistY = field.speakerPositions[i].getCartesian().y - source.position->getCartesian().y;
        squaredDistY *= squaredDistY;
        distZ = std::abs(field.speakerPositions[i].getCartesian().z - source.position->getCartesian().z);
        auto const squaredDistZ = distZ * distZ;

        distFromSource = std::sqrt(squaredDistX + squaredDistY + squaredDistZ);

        distXYPlane = std::sqrt(squaredDistX + squaredDistY);

        auto const gain{ trilinearInterpolation(field.amplitudeMatrix[i], x, y, z) };

        auto const gainNoSpan{ std::pow(gain, field.fieldExponent) };
        auto const gainFullElevSpan{ std::pow(gain, field.fieldExponent * distXYPlane) };

        auto const gainFullAzimuthSpan{ std::pow(gain, field.fieldExponent * distZ) };

        auto const azimuthElevationGain{ std::pow(gain, field.fieldExponent * sumAziElevSpans * distFromSource) };

        const float finalElevationGain{ (gainFullElevSpan - gainNoSpan)
                                            * std::pow(sourceElevationSpan, finalElevSpanExponent * distFromSource)
                                        + gainNoSpan };

        const float finalAzimuthGain{ (gainFullAzimuthSpan - gainNoSpan)
                                          * std::pow(sourceAzimuthSpan, finalAzimuthSpanExponent * distFromSource)
                                      + gainNoSpan };

        gains[i] = sumAziElevSpans * finalAzimuthGain + sumAziElevSpans * finalElevationGain + azimuthElevationGain;
    }

    auto const sum{ std::reduce(gains, gains + field.speakerPositions.size(), 0.0f, std::plus()) };

    if (sum > 0.0f) {
        // (pow(2.0, (1.0 - rad))) for energy spreading when moving toward the center.
        auto const radius{ std::sqrt(std::pow(source.position->getCartesian().x, 2.0f)
                                     + std::pow(source.position->getCartesian().y, 2.0f)
                                     + std::pow(source.position->getCartesian().z, 2.0f)) };
        auto const comp = radius < 1.0f ? std::pow(2.0f, 1.0f - radius) : 1.0f;
        // normalization (1.0 / sum) and compensation
        auto const norm = 1.0f / sum * comp;
        std::transform(gains, gains + field.speakerPositions.size(), gains, [norm](float & gain) {
            return gain * norm;
        });
    }
}
} // namespace

//==============================================================================
size_t MbapField::getNumSpeakers() const
{
    return speakerPositions.size();
}

//==============================================================================
void MbapField::reset()
{
    outputOrder.clear();
    amplitudeMatrix.clear();
}

//==============================================================================
MbapField mbapInit(SpeakersData const & speakers)
{
    std::vector<Position> tempSpeakerPositions;
    tempSpeakerPositions.reserve(narrow<std::size_t>(speakers.size()));

    std::vector<MbapSpeaker> MbapSpeakers{};
    MbapSpeakers.reserve(narrow<std::size_t>(speakers.size()));

    for (auto const & speaker : speakers) {
        if (speaker.data.isDirectOutOnly) {
            continue;
        }

        MbapSpeaker const newSpeaker{ speaker.data.position, speaker.patch };
        MbapSpeakers.push_back(newSpeaker);
    }

    auto const spk{ mbapPositionsFromSpeakers(&MbapSpeakers[0], MbapSpeakers.size()) };
    for (auto speaker : spk) {
        tempSpeakerPositions.emplace_back(speaker);
    }

    auto field{ createField(tempSpeakerPositions) };

    std::transform(MbapSpeakers.cbegin(),
                   MbapSpeakers.cend(),
                   std::back_inserter(field.outputOrder),
                   [](MbapSpeaker const & speaker) { return speaker.outputPatch; });

    return field;
}

//==============================================================================
void mbap(SourceData const & source, SpeakersSpatGains & gains, MbapField const & field)
{
    assert(source.position);

    std::array<float, MAX_NUM_SPEAKERS> tempGains{};

    computeGains(field, source, tempGains.data());

    for (size_t i{}; i < field.getNumSpeakers(); ++i) {
        auto const & outputPatch{ field.outputOrder[i] };
        gains[outputPatch] = tempGains[i];
    }
}

} // namespace Gris
