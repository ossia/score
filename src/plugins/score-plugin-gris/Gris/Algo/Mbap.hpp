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

#pragma once

#include <Gris/Algo/Types.hpp>

#include <Gris/Algo/SpeakerSetup.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace Gris
{
struct SpeakerData;

static auto constexpr MBAP_MATRIX_SIZE = 64;
using matrix_t
    = std::array<std::array<std::array<float, MBAP_MATRIX_SIZE + 1>, MBAP_MATRIX_SIZE + 1>, MBAP_MATRIX_SIZE + 1>;

struct MbapSpeaker {
    Position position{};
    output_patch_t outputPatch{};
};

//==============================================================================
struct MbapField {
    std::vector<output_patch_t> outputOrder; /**< Physical output order. */
    float fieldExponent;                     /**< Speaker gain exponent speakers. */
    std::vector<matrix_t> amplitudeMatrix;   /**< Arrays of amplitude values [spk][x][y][z]. */
    std::vector<Position> speakerPositions;  /**< Array of speakers. */
    //==============================================================================
    [[nodiscard]] size_t getNumSpeakers() const;
    void reset();
};

/**
 * Creates the amplitude field according to the position of speakers.
 */
MbapField mbapInit(SpeakersData const & speakers);

/** \brief Calculates the gain of the outputs for a source's position.
 *
 * This function uses the position `pos` to retrieve the gain for every
 * output from the field and fill the array of float `gains`.
 * The user must provide the array of float and is responsible of its
 * memory. This array can be passed to the audio processing function
 * to control the gain of the signal outputs.
 */
void mbap(SourceData const & source, SpeakersSpatGains & gains, MbapField const & field);

} // namespace Gris
