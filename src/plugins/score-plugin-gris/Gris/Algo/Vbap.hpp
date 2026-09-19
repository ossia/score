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

/*
 * Functions for 3D VBAP processing based on work by Ville Pulkki.
 * (c) Ville Pulkki - 2.2.1999 Helsinki University of Technology.
 * Updated by belangeo, 2017.
 * Updated by Samuel Béland, 2021.
 */

#pragma once

#include <Gris/Algo/Types.hpp>
#include <vector>

#include <Gris/Algo/SpeakerSetup.hpp>



#include <array>
#include <cstddef>
#include <memory>

namespace Gris
{
struct SourceData;

using InverseMatrix = std::array<float, 9>;

/* A struct for a loudspeaker triplet or pair (set). */
struct SpeakerSet {
    std::array<output_patch_t, 3> speakerNos;
    InverseMatrix invMx;
    std::array<float, 3> setGains;
    float smallestWt;
    int negGAm;
};

/* VBAP structure of n loudspeaker panning */
struct VbapData {
    std::array<output_patch_t, MAX_NUM_SPEAKERS> outputPatches{}; /* Physical outputs (starts at 1). */
    std::array<float, MAX_NUM_SPEAKERS> gainsSmoothing{};         /* Loudspeaker gains smoothing. */
    std::size_t dimension{};                                      /* Dimensions, 2 or 3. */
    std::vector<SpeakerSet> speakerSets{};                        /* Loudspeaker triplet structure. */
    int numOutputPatches{};                                       /* Number of output patches. */
    int numSpeakers{};                                            /* Number of loudspeakers. */
    Position direction{};
    CartesianVector spreadingVector{}; /* Spreading vector. */
};

std::unique_ptr<VbapData> vbapInit(std::array<Position, MAX_NUM_SPEAKERS> & speakers,
                                   int count,
                                   int dimensions,
                                   std::array<output_patch_t, MAX_NUM_SPEAKERS> const & outputPatches);

/* Calculates gain factors using loudspeaker setup and angle direction.
 */
void vbapCompute(SourceData const & source, SpeakersSpatGains & gains, VbapData & data) noexcept;

std::vector<Triplet> vbapExtractTriplets(VbapData const & data);

} // namespace Gris
