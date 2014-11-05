#pragma once


class SynchronisationPolicy
{

};

using SynchronisationPolicy_p = std::unique_ptr<SynchronisationPolicy>;
/**
 * @brief The NoSync class
 *
 * Arrivent quand ils veulent
 */
class NoSync : public SynchronisationPolicy
{

};

/**
 * @brief The AllSync class
 *
 * Attente de tout le monde
 */
class AllSync : public SynchronisationPolicy
{

};

/**
 * @brief The OneSync class
 *
 * Un seul compte pour la suite
 */
class OneSync : public SynchronisationPolicy
{

};
