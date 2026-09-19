/*
 * Functions for 3D VBAP processing based on work by Ville Pulkki.
 * (c) Ville Pulkki - 2.2.1999 Helsinki University of Technology.
 * Updated by belangeo, 2017.
 * Updated by Samuel Béland, 2021.
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

#include <Gris/Algo/Vbap.hpp>
#include <Gris/Algo/Types.hpp>


#include <Gris/Algo/SpeakerSetup.hpp>




#include <cassert>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <numeric>
#include <vector>

namespace Gris
{
namespace
{
//==============================================================================
struct TripletData {
    std::array<std::size_t, 3> tripletSpeakerNumber{}; /* Triplet speaker numbers */
    std::array<float, 9> tripletInverseMatrix{};       /* Triplet inverse matrix */
};

using triplet_list_t = std::vector<TripletData>;

//==============================================================================
/* Selects a vector base of a virtual source.
 * Calculates gain factors in that base.
 *
 * This is called when a source is moved.
 */
static void computeGains(std::vector<SpeakerSet> & sets,
                         SpeakersSpatGains & gains,
                         int const numSpeakers,
                         Position const & position,
                         std::size_t const dim) noexcept
{
    // SpatGRIS indexes `sets` through juce::Array::operator[], which returns a
    // default-constructed element when out of range; std::vector does not, so
    // the empty case (a setup with no usable triplet or pair) is handled here.
    if (sets.empty()) {
        gains.fill(0.0f);
        return;
    }

    float vec[3]{};
    /* Direction of the virtual source in cartesian coordinates. */
    vec[0] = position.getCartesian().x;
    vec[1] = position.getCartesian().y;
    vec[2] = position.getCartesian().z;

    for (auto & set : sets) {
        set.setGains[0] = 0.0f;
        set.setGains[1] = 0.0f;
        set.setGains[2] = 0.0f;
        set.smallestWt = 1000.0f;
        set.negGAm = 0;
    }

    for (auto & set : sets) {
        for (std::size_t j{}; j < dim; ++j) {
            for (std::size_t k{}; k < dim; ++k) {
                set.setGains[j] += vec[k] * set.invMx[(dim * j + k)];
            }
            if (set.smallestWt > set.setGains[j])
                set.smallestWt = set.setGains[j];
            if (set.setGains[j] < -0.05f)
                ++set.negGAm;
        }
    }

    int j{};
    auto tmp = sets[0].smallestWt;
    auto tmp2 = sets[0].negGAm;
    for (std::size_t i{ 1 }; i < sets.size(); ++i) {
        if (sets[i].negGAm < tmp2) {
            tmp = sets[i].smallestWt;
            tmp2 = sets[i].negGAm;
            j = narrow<int>(i);
        } else if (sets[i].negGAm == tmp2) {
            if (sets[i].smallestWt > tmp) {
                tmp = sets[i].smallestWt;
                tmp2 = sets[i].negGAm;
                j = narrow<int>(i);
            }
        }
    }

    if (sets[j].setGains[0] <= 0.0f && sets[j].setGains[1] <= 0.0f && sets[j].setGains[2] <= 0.0f) {
        sets[j].setGains[0] = 1.0f;
        sets[j].setGains[1] = 1.0f;
        sets[j].setGains[2] = 1.0f;
    }

    auto * rawGains{ gains.data() };
#if DEBUG
    if (sets.empty())
        return;
#endif
    rawGains[sets[j].speakerNos[0].get() - 1] = sets[j].setGains[0];
    rawGains[sets[j].speakerNos[1].get() - 1] = sets[j].setGains[1];

    if (dim == 3) {
        rawGains[sets[j].speakerNos[2].get() - 1] = sets[j].setGains[2];
    }

    for (int i{}; i < numSpeakers; ++i) {
        if (rawGains[i] < 0.0f) {
            rawGains[i] = 0.0f;
        }
    }
}

//==============================================================================
/* Returns 1 if there is loudspeaker(s) inside given ls triplet. */
static bool testTripletContainsSpeaker(std::size_t const a,
                                       std::size_t const b,
                                       std::size_t const c,
                                       std::array<Position, MAX_NUM_SPEAKERS> const & speakers,
                                       std::size_t const numSpeakers) noexcept
{
    InverseMatrix inverseMatrix{};

    auto const * const lp1 = &speakers[a].getCartesian();
    auto const * const lp2 = &speakers[b].getCartesian();
    auto const * const lp3 = &speakers[c].getCartesian();

    /* Matrix inversion. */
    auto const inverseDeterminant
        = 1.0f
          / (lp1->x * (lp2->y * lp3->z - lp2->z * lp3->y) - lp1->y * (lp2->x * lp3->z - lp2->z * lp3->x)
             + lp1->z * (lp2->x * lp3->y - lp2->y * lp3->x));

    inverseMatrix[0] = (lp2->y * lp3->z - lp2->z * lp3->y) * inverseDeterminant;
    inverseMatrix[3] = (lp1->y * lp3->z - lp1->z * lp3->y) * -inverseDeterminant;
    inverseMatrix[6] = (lp1->y * lp2->z - lp1->z * lp2->y) * inverseDeterminant;
    inverseMatrix[1] = (lp2->x * lp3->z - lp2->z * lp3->x) * -inverseDeterminant;
    inverseMatrix[4] = (lp1->x * lp3->z - lp1->z * lp3->x) * inverseDeterminant;
    inverseMatrix[7] = (lp1->x * lp2->z - lp1->z * lp2->x) * -inverseDeterminant;
    inverseMatrix[2] = (lp2->x * lp3->y - lp2->y * lp3->x) * inverseDeterminant;
    inverseMatrix[5] = (lp1->x * lp3->y - lp1->y * lp3->x) * -inverseDeterminant;
    inverseMatrix[8] = (lp1->x * lp2->y - lp1->y * lp2->x) * inverseDeterminant;

    for (std::size_t i{}; i < numSpeakers; ++i) {
        if (i != a && i != b && i != c) {
            auto thisInside{ true };
            for (std::size_t j{}; j < 3; ++j) {
                auto tmp = speakers[i].getCartesian().x * inverseMatrix[0 + j * 3];
                tmp += speakers[i].getCartesian().y * inverseMatrix[1 + j * 3];
                tmp += speakers[i].getCartesian().z * inverseMatrix[2 + j * 3];
                if (tmp < -0.001f) {
                    thisInside = false;
                    break;
                }
            }
            if (thisInside) {
                return true;
            }
        }
    }
    return false;
}

//==============================================================================
/* Checks if two lines intersect on 3D sphere see theory in paper
 * Pulkki, V. Lokki, T. "Creating Auditory Displays with Multiple
 * Loudspeakers Using VBAP: A Case Study with DIVA Project" in
 * International Conference on Auditory Displays -98.
 * E-mail Ville.Pulkki@hut.fi if you want to have that paper. */
static bool linesIntersect(std::size_t const i,
                           std::size_t const j,
                           std::size_t const k,
                           std::size_t const l,
                           std::array<Position, MAX_NUM_SPEAKERS> const & speakers) noexcept
{
    auto const v1{ speakers[i].getCartesian().crossProduct(speakers[j].getCartesian()) };
    auto const v2{ speakers[k].getCartesian().crossProduct(speakers[l].getCartesian()) };
    auto const v3{ v1.crossProduct(v2) };
    auto const negV3{ -v3 };

    auto const distIj = speakers[i].getCartesian().angleWith(speakers[j].getCartesian());
    auto const distKl = speakers[k].getCartesian().angleWith(speakers[l].getCartesian());
    auto const distIv3 = speakers[i].getCartesian().angleWith(v3);
    auto const distJv3 = v3.angleWith(speakers[j].getCartesian());
    auto const distInv3 = speakers[i].getCartesian().angleWith(negV3);
    auto const distJnv3 = negV3.angleWith(speakers[j].getCartesian());
    auto const distKv3 = speakers[k].getCartesian().angleWith(v3);
    auto const distLv3 = v3.angleWith(speakers[l].getCartesian());
    auto const distKnv3 = speakers[k].getCartesian().angleWith(negV3);
    auto const distLnv3 = negV3.angleWith(speakers[l].getCartesian());

    /*One of loudspeakers is close to crossing point, don't do anything.*/
    if (std::abs(distIv3) <= 0.01f || std::abs(distJv3) <= 0.01f || std::abs(distKv3) <= 0.01f
        || std::abs(distLv3) <= 0.01f || std::abs(distInv3) <= 0.01f || std::abs(distJnv3) <= 0.01f
        || std::abs(distKnv3) <= 0.01f || std::abs(distLnv3) <= 0.01f) {
        return false;
    }

    if ((std::abs(distIj - (distIv3 + distJv3)) <= 0.01f && std::abs(distKl - (distKv3 + distLv3)) <= 0.01f)
        || (std::abs(distIj - (distInv3 + distJnv3)) <= 0.01f && std::abs(distKl - (distKnv3 + distLnv3)) <= 0.01f)) {
        return true;
    }
    return false;
}

//==============================================================================
static void spreadGains3d(SourceData const & source, SpeakersSpatGains & gains, VbapData & data) noexcept
{
    int ind;
    static constexpr auto NUM = 4;
    radians_t newAzimuth;
    radians_t newElevation;
    float sum{};
    SpeakersSpatGains tmpGains{};

    auto const spAzi = std::clamp(source.azimuthSpan, 0.0f, 1.0f);
    auto const spEle = std::clamp(source.zenithSpan, 0.0f, 1.0f);

    // If both sp_azi and sp_ele are active, we want to put a virtual source at
    // (azi, ele +/- elevationDev) and (azi +/- azimuthDev, ele) locations.
    auto const kNum{ spAzi > 0.0 && spEle > 0.0 ? 8 : 4 };

    auto * rawGains{ gains.data() };

    for (int i{}; i < NUM; ++i) {
        auto const iFloat{ narrow<float>(i) };
        auto const compensation{ std::pow(10.0f, (iFloat + 1.0f) * -3.0f * 0.05f) };
        radians_t const azimuthDev{ degrees_t{ 45.0f } * (iFloat + 1.0f) * spAzi };
        radians_t const elevationDev{ degrees_t{ 22.5f } * (iFloat + 1.0f) * spEle };
        for (int k{}; k < kNum; ++k) {
            switch (k) {
            case 0:
                newAzimuth = data.direction.getPolar().azimuth + azimuthDev;
                newElevation = data.direction.getPolar().elevation + elevationDev;
                break;
            case 1:
                newAzimuth = data.direction.getPolar().azimuth - azimuthDev;
                newElevation = data.direction.getPolar().elevation - elevationDev;
                break;
            case 2:
                newAzimuth = data.direction.getPolar().azimuth + azimuthDev;
                newElevation = data.direction.getPolar().elevation - elevationDev;
                break;
            case 3:
                newAzimuth = data.direction.getPolar().azimuth - azimuthDev;
                newElevation = data.direction.getPolar().elevation + elevationDev;
                break;
            case 4:
                newAzimuth = data.direction.getPolar().azimuth;
                newElevation = data.direction.getPolar().elevation + elevationDev;
                break;
            case 5:
                newAzimuth = data.direction.getPolar().azimuth;
                newElevation = data.direction.getPolar().elevation - elevationDev;
                break;
            case 6:
                newAzimuth = data.direction.getPolar().azimuth + azimuthDev;
                newElevation = data.direction.getPolar().elevation;
                break;
            case 7:
                newAzimuth = data.direction.getPolar().azimuth - azimuthDev;
                newElevation = data.direction.getPolar().elevation;
                break;
            default:
                assert(false);
            }

            newElevation = std::clamp(newElevation, radians_t{}, HALF_PI);
            newAzimuth = newAzimuth.balanced();
            PolarVector const spreadAngle{ newAzimuth, newElevation, 1.0f };
            computeGains(data.speakerSets, tmpGains, data.numSpeakers, Position{ spreadAngle }, data.dimension);
            std::transform(gains.cbegin(),
                           gains.cend(),
                           tmpGains.cbegin(),
                           gains.begin(),
                           [compensation](float const a, float const b) { return a + b * compensation; });
        }
    }

    if (spAzi > 0.8f && spEle > 0.8f) {
        auto const compensation = (spAzi - 0.8f) / 0.2f * (spEle - 0.8f) / 0.2f * 10.0f;
        for (int i{}; i < data.numOutputPatches; ++i) {
            rawGains[data.outputPatches[narrow<std::size_t>(i)].get() - 1] += compensation;
        }
    }

    for (int i{}; i < data.numOutputPatches; ++i) {
        ind = data.outputPatches[narrow<std::size_t>(i)].get() - 1;
        sum += rawGains[ind] * rawGains[ind];
    }
    sum = std::sqrt(sum);
    for (int i{}; i < data.numOutputPatches; ++i) {
        ind = data.outputPatches[narrow<std::size_t>(i)].get() - 1;
        rawGains[ind] /= sum;
    }
}

//==============================================================================
static void spreadGains2d(SourceData const & source, SpeakersSpatGains & gains, VbapData & data)
{
    int i;
    static constexpr auto NUM = 4;
    radians_t newAzimuth{};
    auto const cnt{ data.numSpeakers };
    SpeakersSpatGains tmpGains{};
    auto * rawGains{ gains.data() };
    float sum = 0.0;

    auto const azimuthSpread{ std::clamp(source.azimuthSpan, 0.0f, 1.0f) };

    for (i = 0; i < NUM; i++) {
        auto const compensation{ std::pow(10.0f, narrow<float>(i + 1) * -3.0f * 0.05f) };
        radians_t const azimuthDeviation{ degrees_t{ narrow<float>(i + 1) * azimuthSpread * 45.0f } };
        for (int k{}; k < 2; ++k) {
            if (k == 0) {
                newAzimuth = data.direction.getPolar().azimuth + azimuthDeviation;
            } else if (k == 1) {
                newAzimuth = data.direction.getPolar().azimuth - azimuthDeviation;
            }
            newAzimuth = newAzimuth.balanced();
            PolarVector const spreadAngle{ newAzimuth, radians_t{}, 1.0f };
            computeGains(data.speakerSets, tmpGains, data.numSpeakers, Position{ spreadAngle }, data.dimension);
            auto const * rawTmpGains{ tmpGains.data() };
            for (int j{}; j < cnt; ++j) {
                rawGains[j] += rawTmpGains[j] * compensation;
            }
        }
    }

    for (i = 0; i < cnt; i++) {
        sum += rawGains[i] * rawGains[i];
    }
    sum = std::sqrt(sum);
    for (i = 0; i < cnt; i++) {
        rawGains[i] /= sum;
    }
}

//==============================================================================
static void sortSpeakers2d(std::array<Position, MAX_NUM_SPEAKERS> & speakers,
                           std::array<std::size_t, MAX_NUM_SPEAKERS> & sortedSpeakers,
                           std::size_t const numSpeakers)
{
    for (auto & speaker : speakers) {
        speaker = speaker.withBalancedAzimuth(speaker.getPolar().azimuth);
    }

    std::iota(sortedSpeakers.begin(), sortedSpeakers.begin() + numSpeakers, 0);

    std::sort(sortedSpeakers.begin(),
              sortedSpeakers.begin() + numSpeakers,
              [&](std::size_t const a, std::size_t const b) {
                  return speakers[a].getPolar().azimuth < speakers[b].getPolar().azimuth;
              });
}

//==============================================================================
static int computeInverseMatrix2d(radians_t const azi1, radians_t const azi2, float invMat[4])
{
    // SpatGRIS uses juce::dsp::FastMathApproximations here; std::cos/std::sin
    // are used instead so that no JUCE is needed. They are more accurate, not
    // less, and this only feeds a 2x2 determinant test.
    auto const x1 = std::cos(azi1.get());
    auto const x2 = std::sin(azi1.get());
    auto const x3 = std::cos(azi2.get());
    auto const x4 = std::sin(azi2.get());
    auto const det = x1 * x4 - x3 * x2;
    if (std::abs(det) <= 0.001f) {
        invMat[0] = 0.0f;
        invMat[1] = 0.0f;
        invMat[2] = 0.0f;
        invMat[3] = 0.0f;
        return 0;
    }
    invMat[0] = x4 / det;
    invMat[1] = -x3 / det;
    invMat[2] = -x2 / det;
    invMat[3] = x1 / det;
    return 1;
}

//==============================================================================
/* Selects the loudspeaker pairs, calculates the inversion
 * matrices and stores the data to a global array.
 */
static void generateTuplets(std::array<Position, MAX_NUM_SPEAKERS> & speakers,
                            triplet_list_t & triplets,
                            std::size_t const numSpeakers)
{
    static constexpr radians_t MAX_SPREAD_ANGLE{ degrees_t{ 170.0f } };

    std::array<std::size_t, MAX_NUM_SPEAKERS> exist{};
    std::array<std::size_t, MAX_NUM_SPEAKERS> sortedSpeakers{};
    /* Sort loudspeakers according their azimuth angle. */
    sortSpeakers2d(speakers, sortedSpeakers, numSpeakers);

    /* Adjacent loudspeakers are the loudspeaker pairs to be used. */
    float inverseMatrix[MAX_NUM_SPEAKERS][4]{};
    for (std::size_t i{}; i < (numSpeakers - 1); ++i) {
        if (speakers[sortedSpeakers[i + 1]].getPolar().azimuth - speakers[sortedSpeakers[i]].getPolar().azimuth
            <= MAX_SPREAD_ANGLE) {
            if (computeInverseMatrix2d(speakers[sortedSpeakers[i]].getPolar().azimuth,
                                       speakers[sortedSpeakers[i + 1]].getPolar().azimuth,
                                       inverseMatrix[i])
                != 0) {
                exist[i] = 1;
            }
        }
    }

    if (TWO_PI - speakers[sortedSpeakers[numSpeakers - 1]].getPolar().azimuth
            + speakers[sortedSpeakers[0]].getPolar().azimuth
        <= MAX_SPREAD_ANGLE) {
        if (computeInverseMatrix2d(speakers[sortedSpeakers[numSpeakers - 1]].getPolar().azimuth,
                                   speakers[sortedSpeakers[0]].getPolar().azimuth,
                                   inverseMatrix[numSpeakers - 1])
            != 0) {
            exist[narrow<std::size_t>(numSpeakers - 1)] = 1;
        }
    }

    for (std::size_t i{}; i < numSpeakers - 1; ++i) {
        if (exist[i] == 1) {
            TripletData newTripletData{};

            newTripletData.tripletSpeakerNumber[0] = sortedSpeakers[i] + 1;
            newTripletData.tripletSpeakerNumber[1] = sortedSpeakers[i + 1] + 1;
            for (std::size_t j{}; j < 4; ++j) {
                newTripletData.tripletInverseMatrix[j] = inverseMatrix[i][j];
            }

            triplets.push_back(newTripletData);
        }
    }

    if (exist[numSpeakers - 1] == 1) {
        TripletData newTripletData{};

        newTripletData.tripletSpeakerNumber[0] = sortedSpeakers[numSpeakers - 1] + 1;
        newTripletData.tripletSpeakerNumber[1] = sortedSpeakers[0] + 1;
        for (std::size_t j{}; j < 4; ++j) {
            newTripletData.tripletInverseMatrix[j] = inverseMatrix[numSpeakers - 1][j];
        }

        triplets.push_back(newTripletData);
    }
}

//==============================================================================
/* Calculate volume of the parallelepiped defined by the loudspeaker
 * direction vectors and divide it with total length of the triangle sides.
 * This is used when removing too narrow triangles. */
static float parallelepipedVolumeSideLength(Position const & i, Position const & j, Position const & k) noexcept
{
    auto const length = i.getCartesian().angleWith(j.getCartesian()) + i.getCartesian().angleWith(k.getCartesian())
                        + j.getCartesian().angleWith(k.getCartesian());

    if (length <= 0.00001f) {
        return 0.0f;
    }

    auto const xProduct{ i.getCartesian().crossProduct(j.getCartesian()) };
    auto const volper = std::abs(xProduct.dotProduct(k.getCartesian()));

    return volper / length;
}

//==============================================================================
/* Selects the loudspeaker triplets, and calculates the inversion
 * matrices for each selected triplet. A line (connection) is drawn
 * between each loudspeaker. The lines denote the sides of the
 * triangles. The triangles should not be intersecting. All crossing
 * connections are searched and the longer connection is erased.
 * This yields non-intersecting triangles, which can be used in panning.
 */
static triplet_list_t generateTriplets(std::array<Position, MAX_NUM_SPEAKERS> const & speakers,
                                       std::size_t const numSpeakers)
{
    assert(numSpeakers > 0);

    /*
     * Ok, so the next part of the algorithm has to check vol_p_side_lgth() for EVERY possible speaker triplet. This
     * takes an absurd amount of time for setups bigger than 100 speakers.
     *
     * Luckily, at least two speakers have to be at a similar elevation for the triplet to be valid. Instead of looking
     * for this inside vol_p_side_lgth(), we can take advantage of this fact to reduce the search space :
     *
     * 1- Sort all the speaker indexes according to their elevation.
     *
     * 2- Select every pair of speaker that is within the maximum elevation range and for every other speaker, check for
     * vol_p_side_lgth().
     *
     * -Samuel
     */

    // We first build an array with all the indexes
    std::vector<int> speakerIndexesSortedByElevation{};
    speakerIndexesSortedByElevation.resize(narrow<size_t>(numSpeakers));
    std::iota(std::begin(speakerIndexesSortedByElevation), std::end(speakerIndexesSortedByElevation), 0);

    // ...then we sort it according to the elevation values
    auto const sortIndexesBySpeakerElevation = [speakers](size_t const & indexA, size_t const & indexB) -> bool {
        return speakers[indexA].getPolar().elevation < speakers[indexB].getPolar().elevation;
    };
    std::sort(std::begin(speakerIndexesSortedByElevation),
              std::end(speakerIndexesSortedByElevation),
              sortIndexesBySpeakerElevation);

    // ...then we test for valid triplets ONLY when the elevation difference is within a specified range for two
    // speakers
    auto const connectionsPtr{ std::make_unique<std::array<std::array<bool, MAX_NUM_SPEAKERS>, MAX_NUM_SPEAKERS>>() };
    auto & connections{ *connectionsPtr };
    triplet_list_t triplets{};
    for (size_t i{}; i < speakerIndexesSortedByElevation.size(); ++i) {
        auto const speaker1Index{ speakerIndexesSortedByElevation[i] };
        auto const & speaker1{ speakers[narrow<std::size_t>(speaker1Index)] };
        for (auto j{ i + 1 }; j < speakerIndexesSortedByElevation.size(); ++j) {
            auto const speaker2Index{ speakerIndexesSortedByElevation[j] };
            auto const & speaker2{ speakers[narrow<std::size_t>(speaker2Index)] };

            // TODO : disabling this check seems to have solved a lot of incomplete triangular meshes that used to
            // happen on various random speaker setups.

            // static constexpr radians_t MAX_ELEVATION_DIFF{ degrees_t{ 10.0f } };
            // if (speaker2.getPolar().elevation - speaker1.getPolar().elevation > MAX_ELEVATION_DIFF) {
            //    // The elevation difference is only going to get greater : we can move the 1st speaker and reset the
            //    // other loops
            //    break;
            //}

            for (size_t k{}; k < speakerIndexesSortedByElevation.size(); ++k) {
                if (k >= i && k <= j) {
                    // If k is between i and j, it means that i and k are within the elevation threshold (as well as k
                    // and j), so they are going to get checked anyway. We also need not to include i or j twice!
                    continue;
                }
                auto const speaker3Index{ speakerIndexesSortedByElevation[k] };
                auto const & speaker3{ speakers[narrow<std::size_t>(speaker3Index)] };
                auto const parallelepipedVolume{ parallelepipedVolumeSideLength(speaker1, speaker2, speaker3) };
                static constexpr auto MIN_VOL_P_SIDE_LENGTH = 0.01f;
                if (parallelepipedVolume > MIN_VOL_P_SIDE_LENGTH) {
                    connections[narrow<std::size_t>(speaker1Index)][narrow<std::size_t>(speaker2Index)] = true;
                    connections[narrow<std::size_t>(speaker2Index)][narrow<std::size_t>(speaker1Index)] = true;
                    connections[narrow<std::size_t>(speaker1Index)][narrow<std::size_t>(speaker3Index)] = true;
                    connections[narrow<std::size_t>(speaker3Index)][narrow<std::size_t>(speaker1Index)] = true;
                    connections[narrow<std::size_t>(speaker2Index)][narrow<std::size_t>(speaker3Index)] = true;
                    connections[narrow<std::size_t>(speaker3Index)][narrow<std::size_t>(speaker2Index)] = true;

                    TripletData newTripletData{};

                    newTripletData.tripletSpeakerNumber[0] = narrow<std::size_t>(speaker1Index);
                    newTripletData.tripletSpeakerNumber[1] = narrow<std::size_t>(speaker2Index);
                    newTripletData.tripletSpeakerNumber[2] = narrow<std::size_t>(speaker3Index);

                    triplets.push_back(newTripletData);
                }
            }
        }
    }

    /* Calculate distances between all lss and sorting them. */
    auto tableSize = (numSpeakers - 1) * numSpeakers / 2;
    std::vector<float> distanceTable{};
    distanceTable.resize(tableSize);
    std::fill(std::begin(distanceTable), std::end(distanceTable), 100000.0f);

    std::vector<std::size_t> distanceTableI{};
    distanceTableI.resize(tableSize);

    std::vector<std::size_t> distanceTableJ{};
    distanceTableJ.resize(tableSize);

    for (std::size_t i{}; i < numSpeakers; ++i) {
        for (auto j{ i + 1 }; j < numSpeakers; ++j) {
            if (connections[i][j]) {
                auto const distance{ speakers[i].getCartesian().angleWith(speakers[j].getCartesian()) };
                std::size_t k{};
                while (distanceTable[k] < distance) {
                    ++k;
                }
                for (auto l{ tableSize - 1 }; l > k; --l) {
                    distanceTable[l] = distanceTable[l - 1];
                    distanceTableI[l] = distanceTableI[l - 1];
                    distanceTableJ[l] = distanceTableJ[l - 1];
                }
                distanceTable[k] = distance;
                distanceTableI[k] = i;
                distanceTableJ[k] = j;
            } else {
                tableSize--;
            }
        }
    }

    /* Disconnecting connections which are crossing shorter ones,
     * starting from shortest one and removing all that cross it,
     * and proceeding to next shortest. */
    for (std::size_t i{}; i < tableSize; ++i) {
        auto const fstLs = distanceTableI[i];
        auto const secLs = distanceTableJ[i];
        if (connections[fstLs][secLs]) {
            for (std::size_t j{}; j < numSpeakers; ++j) {
                for (auto k{ j + 1 }; k < numSpeakers; ++k) {
                    if (j != fstLs && k != secLs && k != fstLs && j != secLs) {
                        if (linesIntersect(fstLs, secLs, j, k, speakers)) {
                            connections[j][k] = false;
                            connections[k][j] = false;
                        }
                    }
                }
            }
        }
    }

    /* Remove triangles which had crossing sides with
     * smaller triangles or include loudspeakers. */
    auto const predicate = [&connections, speakers, numSpeakers](TripletData const & triplet) -> bool {
        auto const & i = triplet.tripletSpeakerNumber[0];
        auto const & j = triplet.tripletSpeakerNumber[1];
        auto const & k = triplet.tripletSpeakerNumber[2];

        if (testTripletContainsSpeaker(i, j, k, speakers, numSpeakers)) {
            return false;
        }

        return connections[i][j] && connections[i][k] && connections[j][k];
    };
    auto const newTripletEnd{ std::partition(std::begin(triplets), std::end(triplets), predicate) };

    triplets.erase(newTripletEnd, std::end(triplets));
    triplets.shrink_to_fit();
    return triplets;
}

//==============================================================================
/* Calculates the inverse matrices for 3D.
 *
 * After this call, ls_triplets contains the speakers numbers
 * and the inverse matrix needed to compute channel gains.
 */
static void
    computeMatrices3d(triplet_list_t & triplets, std::array<Position, MAX_NUM_SPEAKERS> & speakers, int /*numSpeakers*/)
{
    for (auto & triplet : triplets) {
        auto const * lp1 = &speakers[triplet.tripletSpeakerNumber[0]].getCartesian();
        auto const * lp2 = &speakers[triplet.tripletSpeakerNumber[1]].getCartesian();
        auto const * lp3 = &speakers[triplet.tripletSpeakerNumber[2]].getCartesian();

        /* Matrix inversion. */
        auto & inverseMatrix = triplet.tripletInverseMatrix;
        auto const inverseDet
            = 1.0f
              / (lp1->x * (lp2->y * lp3->z - lp2->z * lp3->y) - lp1->y * (lp2->x * lp3->z - lp2->z * lp3->x)
                 + lp1->z * (lp2->x * lp3->y - lp2->y * lp3->x));

        inverseMatrix[0] = (lp2->y * lp3->z - lp2->z * lp3->y) * inverseDet;
        inverseMatrix[3] = (lp1->y * lp3->z - lp1->z * lp3->y) * -inverseDet;
        inverseMatrix[6] = (lp1->y * lp2->z - lp1->z * lp2->y) * inverseDet;
        inverseMatrix[1] = (lp2->x * lp3->z - lp2->z * lp3->x) * -inverseDet;
        inverseMatrix[4] = (lp1->x * lp3->z - lp1->z * lp3->x) * inverseDet;
        inverseMatrix[7] = (lp1->x * lp2->z - lp1->z * lp2->x) * -inverseDet;
        inverseMatrix[2] = (lp2->x * lp3->y - lp2->y * lp3->x) * inverseDet;
        inverseMatrix[5] = (lp1->x * lp3->y - lp1->y * lp3->x) * -inverseDet;
        inverseMatrix[8] = (lp1->x * lp2->y - lp1->y * lp2->x) * inverseDet;
    }
}
} // namespace

//==============================================================================
std::unique_ptr<VbapData> vbapInit(std::array<Position, MAX_NUM_SPEAKERS> & speakers,
                                   int const count,
                                   int const dimensions,
                                   std::array<output_patch_t, MAX_NUM_SPEAKERS> const & outputPatches)
{
    int offset{};
    triplet_list_t triplets{};
    auto data = std::make_unique<VbapData>();
    if (dimensions == 3) {
        triplets = generateTriplets(speakers, narrow<std::size_t>(count));
        computeMatrices3d(triplets, speakers, count);
        offset = 1;
    } else if (dimensions == 2) {
        generateTuplets(speakers, triplets, narrow<std::size_t>(count));
    }

    data->numOutputPatches = count;
    for (std::size_t i{}; i < narrow<std::size_t>(count); i++) {
        data->outputPatches[i] = outputPatches[i];
    }

    data->dimension = narrow<std::size_t>(dimensions);
    data->numSpeakers = narrow<int>(speakers.size());

    for (auto const & triplet : triplets) {
        SpeakerSet newSet{};
        for (std::size_t j{}; j < data->dimension; ++j) {
            newSet.speakerNos[j] = outputPatches[triplet.tripletSpeakerNumber[j] + narrow<std::size_t>(offset) - 1];
        }
        for (std::size_t j{}; j < data->dimension * data->dimension; ++j) {
            newSet.invMx[j] = triplet.tripletInverseMatrix[j];
        }
        data->speakerSets.push_back(newSet);
    }

    return data;
}

//==============================================================================
void vbapCompute(SourceData const & source, SpeakersSpatGains & gains, VbapData & data) noexcept
{
    assert(source.position);
    data.direction = source.position->getPolar();
    std::fill(gains.begin(), gains.end(), 0.0f);
    computeGains(data.speakerSets, gains, data.numSpeakers, data.direction, data.dimension);
    if (data.dimension == 3) {
        if (source.azimuthSpan > 0 || source.zenithSpan > 0) {
            spreadGains3d(source, gains, data);
        }
    } else {
        if (source.azimuthSpan > 0) {
            spreadGains2d(source, gains, data);
        }
    }
}

//==============================================================================
std::vector<Triplet> vbapExtractTriplets(VbapData const & data)
{
    std::vector<Triplet> result{};
    result.reserve(data.speakerSets.size());
    for (auto const & set : data.speakerSets) {
        Triplet triplet{};

        triplet.id1 = output_patch_t{ set.speakerNos[0] };
        triplet.id2 = output_patch_t{ set.speakerNos[1] };
        triplet.id3 = output_patch_t{ set.speakerNos[2] };

        assert(isLegalOutputPatch(triplet.id1) && isLegalOutputPatch(triplet.id2)
               && isLegalOutputPatch(triplet.id3));

        result.push_back(triplet);
    }
    return result;
}

} // namespace Gris
