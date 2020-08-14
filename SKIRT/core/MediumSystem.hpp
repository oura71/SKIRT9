/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#ifndef MEDIUMSYSTEM_HPP
#define MEDIUMSYSTEM_HPP

#include "Array.hpp"
#include "DustEmissionOptions.hpp"
#include "DustSelfAbsorptionOptions.hpp"
#include "ExtinctionOnlyOptions.hpp"
#include "LyaOptions.hpp"
#include "MaterialMix.hpp"
#include "Medium.hpp"
#include "PhotonPacketOptions.hpp"
#include "SimulationItem.hpp"
#include "SpatialGrid.hpp"
#include "Table.hpp"
class Configuration;
class PhotonPacket;
class Random;
class SpatialGridPath;
class WavelengthGrid;

//////////////////////////////////////////////////////////////////////

/** An instance of the MediumSystem class represents a complete medium system, which is the
    superposition of one or more transfer media. Each individual medium represents a spatial
    density distribution and defines the material properties of the medium at each location. While
    the specific material properties may vary with location, the fundamental material type must be
    the same throughout the spatial domain for each medium.

    In addition to the media input model, the MediumSystem class includes the spatial grid that
    tessellates the spatial domain of the simulation into cells, and manages the medium state and
    the radiation field for each spatial cell in this grid.

    The current implementation focuses on electrons and dust (i.e. there are no provisions for gas)
    and assumes that the density distribution and material mix remain constant after setup (i.e.
    there are no iterations for dust destruction). This will change as additional features are
    added.

    The medium state includes the following information for each cell in the spatial grid: the
    number density in the cell per medium component; (a pointer to) the corresponding material mix
    for each medium component; the aggregate bulk velocity of the material in the cell; the
    magnetic field vector in the cell, and the volume of the cell.

    The contribution to the radation field for each spatial cell and for each wavelength in the
    simulation's radiation field wavelength grid is traced separately for primary and secondary
    sources. This avoids the need for repeating primary emission during dust-temperature
    convergence iterations. At all times, the sum of the primary and secondary contributions
    represents the radiation field to be used as input for calculations. There is a third,
    temporary table that serves as a target for storing the secondary radiation field so that the
    "stable" primary and secondary tables remain available for calculating secondary emission
    spectra while shooting secondary photons through the grid. */
class MediumSystem : public SimulationItem
{
    ITEM_CONCRETE(MediumSystem, SimulationItem, "a medium system")
        ATTRIBUTE_TYPE_ALLOWED_IF(MediumSystem, "!NoMedium")

        PROPERTY_ITEM(photonPacketOptions, PhotonPacketOptions, "the photon packet options")
        ATTRIBUTE_DEFAULT_VALUE(photonPacketOptions, "PhotonPacketOptions")
        ATTRIBUTE_RELEVANT_IF(media, "!NoMedium")

        PROPERTY_ITEM(extinctionOnlyOptions, ExtinctionOnlyOptions, "the extinction-only options")
        ATTRIBUTE_DEFAULT_VALUE(extinctionOnlyOptions, "ExtinctionOnlyOptions")
        ATTRIBUTE_RELEVANT_IF(extinctionOnlyOptions, "ExtinctionOnly")

        PROPERTY_ITEM(dustEmissionOptions, DustEmissionOptions, "the dust emission options")
        ATTRIBUTE_DEFAULT_VALUE(dustEmissionOptions, "DustEmissionOptions")
        ATTRIBUTE_RELEVANT_IF(dustEmissionOptions, "DustEmission")

        PROPERTY_ITEM(dustSelfAbsorptionOptions, DustSelfAbsorptionOptions, "the dust self-absorption options")
        ATTRIBUTE_DEFAULT_VALUE(dustSelfAbsorptionOptions, "DustSelfAbsorptionOptions")
        ATTRIBUTE_RELEVANT_IF(dustSelfAbsorptionOptions, "DustSelfAbsorption")

        PROPERTY_ITEM(lyaOptions, LyaOptions, "the Lyman-alpha line transfer options")
        ATTRIBUTE_DEFAULT_VALUE(lyaOptions, "LyaOptions")
        ATTRIBUTE_RELEVANT_IF(lyaOptions, "Lya")

        PROPERTY_INT(numDensitySamples, "the number of random density samples for determining spatial cell mass")
        ATTRIBUTE_MIN_VALUE(numDensitySamples, "10")
        ATTRIBUTE_MAX_VALUE(numDensitySamples, "1000")
        ATTRIBUTE_DEFAULT_VALUE(numDensitySamples, "100")
        ATTRIBUTE_DISPLAYED_IF(numDensitySamples, "Level2")

        PROPERTY_ITEM_LIST(media, Medium, "the transfer media")
        ATTRIBUTE_DEFAULT_VALUE(media, "GeometricMedium")
        ATTRIBUTE_REQUIRED_IF(media, "!NoMedium")

        PROPERTY_ITEM(grid, SpatialGrid, "the spatial grid")
        ATTRIBUTE_DEFAULT_VALUE(grid,
                                "Dimension3:PolicyTreeSpatialGrid;Dimension2:Cylinder2DSpatialGrid;Sphere1DSpatialGrid")

    ITEM_END()

    //============= Construction - Setup - Destruction =============

protected:
    /** This function calculates and stores initial state information for each spatial cell,
        including the cell volume and the number density for each medium as defined by the input
        model. If needed for the simulation's configuration, it also allocates one or two radiation
        field data tables that have a bin for each spatial cell in the simulation and for each bin
        in the wavelength grid returned by the Configuration::radiationFieldWLG() function. */
    void setupSelfAfter() override;

    //======================== Other Functions =======================

public:
    /** This function returns the dimension of the medium system, which depends on the (lack of)
        symmetry in the geometries of the media it contains (\em not including the spatial grid). A
        value of 1 means spherical symmetry, 2 means axial symmetry and 3 means none of these
        symmetries. The medium with the least symmetry (i.e. the highest dimension) determines the
        result for the whole system. */
    int dimension() const;

    /** This function returns the dimension of the spatial grid held by the medium system. A value
        of 1 means spherical symmetry, 2 means axial symmetry and 3 means none of these symmetries.
        */
    int gridDimension() const;

    /** This function returns the number of media in the medium system. The returned value is valid
        only after setup has been performed. */
    int numMedia() const;

    /** This function returns the number of cells in the spatial grid held by the medium system.
        The returned value is valid only after setup has been performed. */
    int numCells() const;

    /** This function returns the volume of the spatial cell with index \f$m\f$. */
    double volume(int m) const;

    /** This function returns the aggregate bulk velocity \f${\boldsymbol{v}}\f$ of the medium in
        spatial cell with index \f$m\f$. If there are multiple media components, the aggregate bulk
        velocity \f${\boldsymbol{v}}\f$ is determined by averaging the respective bulk velocities
        over the corresponding number densities, \f[{\boldsymbol{v}} = \frac{\sum_h n_h
        {\boldsymbol{v}}_h} {\sum_h n_h}.\f] */
    Vec bulkVelocity(int m) const;

    /** This function returns the magnetic field \f${\boldsymbol{B}}\f$ in the spatial cell with
        index \f$m\f$. At most one medium component is allowed to specify a magnetic field. If no
        medium component specifies a magnetic field, this function returns the null vector. */
    Vec magneticField(int m) const;

    /** This function returns the material mix corresponding to the medium component with index
        \f$h\f$ in spatial cell with index \f$m\f$. */
    const MaterialMix* mix(int m, int h) const;

    /** This function returns true if at least one of the media in the medium system has the
        specified fundamental material type (i.e. dust, electrons, or gas). */
    bool hasMaterialType(MaterialMix::MaterialType type) const;

    /** This function returns true if at least one of the media in the medium system contains dust.
        */
    bool hasDust() const { return hasMaterialType(MaterialMix::MaterialType::Dust); }

    /** This function returns true if at least one of the media in the medium system contains
        electrons. */
    bool hasElectrons() const { return hasMaterialType(MaterialMix::MaterialType::Electrons); }

    /** This function returns true if at least one of the media in the medium system contains gas.
        */
    bool hasGas() const { return hasMaterialType(MaterialMix::MaterialType::Gas); }

    /** This function returns true if the medium component with index \f$h\f$ has the specified
        fundamental material type (i.e. dust, electrons, or gas). */
    bool isMaterialType(MaterialMix::MaterialType type, int h) const;

    /** This function returns true if the medium component with index \f$h\f$ contains dust. */
    bool isDust(int h) const { return isMaterialType(MaterialMix::MaterialType::Dust, h); }

    /** This function returns true if the medium component with index \f$h\f$ contains electrons.
        */
    bool isElectrons(int h) const { return isMaterialType(MaterialMix::MaterialType::Electrons, h); }

    /** This function returns true if the medium component with index \f$h\f$ contains gas. */
    bool isGas(int h) const { return isMaterialType(MaterialMix::MaterialType::Gas, h); }

    /** This function returns the number density of the medium component with index \f$h\f$ in
        spatial cell with index \f$m\f$. */
    double numberDensity(int m, int h) const;

    /** This function returns the mass density of the medium component with index \f$h\f$ in
        spatial cell with index \f$m\f$. */
    double massDensity(int m, int h) const;

    /** This function returns the temperature \f$T\f$ of the medium component with index \f$h\f$ in
        the spatial cell with index \f$m\f$. If the specified medium component does not have a
        temperature state variable, the behavior is undefined. */
    double temperature(int m, int h) const;

private:
    /** This function returns the absorption opacity \f$k_h^\text{abs}\f$ at wavelength
        \f$\lambda\f$ of the medium component with index \f$h\f$ in spatial cell with index
        \f$m\f$. Because no photon packet is provided, default values are used for any relevant
        incoming photon packet properties. For example, the radiation is assumed to be unpolarized.
        */
    double opacityAbs(double lambda, int m, int h) const;

    /** This function returns the scattering opacity \f$k_h^\text{sca}\f$ at wavelength
        \f$\lambda\f$ of the medium component with index \f$h\f$ in spatial cell with index
        \f$m\f$. Because no photon packet is provided, default values are used for any relevant
        incoming photon packet properties. For example, the radiation is assumed to be unpolarized.
        */
    double opacitySca(double lambda, int m, int h) const;

    /** This function returns the extinction opacity \f$k_h^\text{ext}\f$ at wavelength
        \f$\lambda\f$ of the medium component with index \f$h\f$ in spatial cell with index
        \f$m\f$. Because no photon packet is provided, default values are used for any relevant
        incoming photon packet properties. For example, the radiation is assumed to be unpolarized.
        */
    double opacityExt(double lambda, int m, int h) const;

public:
    /** This function returns the absorption opacity \f$k^\text{abs}=\sum_h k_h^\text{abs}\f$
        summed over all medium components with the specified material type at wavelength
        \f$\lambda\f$ in spatial cell with index \f$m\f$. Because no photon packet is provided,
        default values are used for any relevant incoming photon packet properties. For example,
        the radiation is assumed to be unpolarized. */
    double opacityAbs(double lambda, int m, MaterialMix::MaterialType type) const;

    /** This function returns the extinction opacity \f$k^\text{ext}=\sum_h k_h^\text{ext}\f$
        summed over all medium components with the specified material type at wavelength
        \f$\lambda\f$ in spatial cell with index \f$m\f$. Because no photon packet is provided,
        default values are used for any relevant incoming photon packet properties. For example,
        the radiation is assumed to be unpolarized. */
    double opacityExt(double lambda, int m, MaterialMix::MaterialType type) const;

    /** This function returns the extinction opacity \f$k^\text{ext}=\sum_h k_h^\text{ext}\f$
        summed over all medium components at wavelength \f$\lambda\f$ in spatial cell with index
        \f$m\f$. Because no photon packet is provided, default values are used for any relevant
        incoming photon packet properties. For example, the radiation is assumed to be unpolarized.
        */
    double opacityExt(double lambda, int m) const;

    /** This function returns the perceived wavelength of the photon packet at the scattering
        interaction distance, taking into account the bulk velocity and Hubble expansion velocity
        in that cell. */
    double perceivedWavelengthForScattering(const PhotonPacket* pp) const;

    /** This function returns the weighted scattering albedo \f[\frac{\sum_h k_h^\text{sca}}
        {\sum_h k_h^\text{ext}}\f] over all medium components at wavelength \f$\lambda\f$ in the
        spatial cell hosting the specified photon packet's scattering event. The opacities are
        calculated at the wavelength perceived by the medium taking into account the bulk velocity
        and Hubble expansion velocity in that cell and taking into account any relevant properties
        of the specified photon packet such as the polarization state. */
    double albedoForScattering(const PhotonPacket* pp) const;

    /** This function calculates the relative weights of the medium components in a scattering
        event, determined by the scattering opacity \f$k_{m,h}^\text{sca}\f$ of the medium
        component \f$h\f$ in the scattering interaction cell \f$m\f$ obtained from the specified
        photon packet. These opacities are calculated at the specified wavelength (which is assumed
        to be the wavelength perceived by the medium in cell \f$m\f$ taking into account the bulk
        velocity and Hubble expansion velocity in that cell) and taking into account any relevant
        properties of the specified photon packet such as the polarization state.

        The resulting weights are normalized to a total of unity and stored in the target array.
        The array is resized appropriately (i.e. to the number of medium components in the
        simulation). The function returns true if normalized weights have been successfully
        calculated, and false if all of the weights are zero (i.e. the photon packet does not
        scatter in this cell). */
    bool weightsForScattering(Array& wv, double lambda, const PhotonPacket* pp) const;

    /** This function calculates the peel-off photon luminosity, polarization state, and wavelength
        shift for the given wavelength, geometry, and incoming photon packet. The specified
        placeholder peel-off photon packet is then launched using this information so that it is
        ready for detection by instruments.

        If there are multiple medium components, the contributions to the luminosity (and if
        polarization is enabled, to the other components of the Stokes vector) are weighted by the
        relative opacities of the various medium components. If more than one component changes the
        wavelength, only the wavelength shift returned by the last one is preserved (for lack of a
        better strategy). */
    void peelOffScattering(double lambda, const Array& wv, Direction bfkobs, Direction bfky, PhotonPacket* pp,
                           PhotonPacket* ppp) const;

    /** This function simulates a random walk scattering event of a photon packet. Most of the
        properties of the photon packet remain unaltered, including the position and the
        luminosity. The properties that change include the number of scattering events experienced
        by the photon packet, which is increased by one, the propagation direction, which is
        generated randomly, the wavelength, which is properly Doppler-shifted for the bulk velocity
        of the medium, and the polarization state, which may be affected by the scattering process.

        If there is only one medium component, the scattering event is governed by the
        corresponding material mix. If there are several components, the function first randomly
        selects a medium component from the list, where the relative weight of each component
        \f$h\f$ is determined by the scattering opacity \f$k_{m,h}^\text{sca}\f$ of the medium
        component in the scattering interaction cell \f$m\f$ obtained from the specified photon
        packet. These opacities are calculated at the wavelength perceived by the medium in cell
        \f$m\f$ taking into account the bulk velocity and Hubble expansion velocity in that cell,
        and taking into account any relevant properties of the incoming photon packet such as the
        polarization state.

        Performing the actual scattering event is delegated to the material mix corresponding to
        the selected medium component in the interaction cell. Refer to the
        MaterialMix::performScattering() function for more information. */
    void simulateScattering(Random* random, PhotonPacket* pp) const;

    /** This function returns the optical depth at the specified wavelength along a path through
        the medium system, taking into account only medium components with the specified material
        type. The starting position and the direction of the path are taken from the specified
        SpatialGridPath object. This function is intended for use from probes and hence is not
        performance-sensitive.

        The function determines the segments of the path \f$(\Delta s)_m\f$ as it crosses the cells
        with indices \f$m\f$ in the spatial grid and calculates the optical depth along the path as
        \f[ \tau_\text{path} = \sum_m (\Delta s)_m \sum_h k_{m,h}^\text{ext}, \f] where
        \f$k_{m,h}^\text{ext}\f$ is the extinction opacity corresponding to the \f$h\f$'th medium
        component in the cell with index \f$m\f$ at the specified wavelength \f$\lambda\f$ and the
        sum over \f$h\f$ runs only over the medium components with the specified material type. */
    double getOpticalDepth(const SpatialGridPath* path, double lambda, MaterialMix::MaterialType type) const;

    /** This function calculates the cumulative optical depth at the end of each path segment along
        a path through the medium system defined by the initial position and direction of the
        specified PhotonPacket object, and stores the results of the calculation into the same
        PhotonPacket object.

        This function is intended for handling random-walk photon packet paths during a
        forced-scattering photon life cycle. Because it is at the heart of the photon life cycle,
        performance is important. Firstly, separating the geometric and optical depth calculations
        seems to be faster, probably due to memory access and caching issues. So the function first
        determines and stores the path segments and then calculates and stores the cumulative
        optical depth at the end of each segment. Secondly, the function implements optimized
        versions for media with spatially constant cross sections.

        With the geometric path information given, the function calculates the optical depth for
        each path segment \f$(\Delta s)_m\f$ as it crosses the spatial cell with index \f$m\f$ as
        \f[ \tau_m = (\Delta s)_m \sum_h k_{m,h}^\text{ext}, \f] where \f$k_{m,h}^\text{ext}\f$ is
        the extinction opacity corresponding to the \f$h\f$'th medium component in the cell with
        index \f$m\f$ and the sum over \f$h\f$ runs over all medium components. The opacities
        \f$k_{m,h}^\text{ext}\f$ are calculated at the wavelength perceived by the medium in cell
        \f$m\f$ taking into account the bulk velocity and Hubble expansion velocity in that cell,
        and taking into account any relevant properties of the incoming photon packet such as the
        polarization state.

        Using these optical depth values per segment, the function determines the cumulative
        optical depth at the segment exit boundaries and stores them into the specified photon
        packet object as well. Note that the optical depth at entry of the initial segment is equal
        to zero by definition. */
    void setOpticalDepths(PhotonPacket* pp) const;

    /** This function calculates the cumulative optical depth and distance at the end of path
        segments along a path through the medium system defined by the initial position and
        direction of the specified PhotonPacket object until the specified interaction optical
        depth has been reached. The function then interpolates the interaction point, stores it in
        the photon packet, and returns true. If the specified interaction optical depth is never
        reached within the path, the function returns false.

        This function is intended for handling random-walk photon packet paths during a photon life
        cycle that does \em not use forced-scattering. In that case there is no need to calculate
        the complete path, substantially boosting performance in high-optical depth media. Because
        the function is at the heart of the photon life cycle, performance is important. Hence it
        implements optimized versions for media with spatially constant cross sections.

        The optical depth for each traversed path segment is calculated as described for the
        setOpticalDepths() function, i.e. at the wavelength perceived by the medium in the cell
        being crossed and taking into account any relevant properties of the incoming photon
        packet. */
    bool setInteractionPoint(PhotonPacket* pp, double tauscat) const;

    /** This function calculates and returns the optical depth (or -1, see "High optical depth
        below") along a path through the medium system defined by the initial position and
        direction of the specified PhotonPacket object and up to the specified distance.

        This function is intended for handling peel-off photon packets during the photon life
        cycle. Because it is at the heart of the photon life cycle, performance is important. Hence
        the function implements optimized versions for media with spatially constant cross
        sections. Furthermore, the calculation is limited to the specified distance along the path.
        More precisely, all path segments with an entry boundary at a cumulative distance along the
        path smaller than the specified distance are included in the calculation, and any remaining
        segments are skipped.

        The optical depth for each traversed path segment is calculated as described for the
        setOpticalDepths() function, i.e. at the wavelength perceived by the medium in the cell
        being crossed and taking into account any relevant properties of the incoming photon
        packet. This process may require storing intermediate results in the photon packet.

        <b>High optical depth</b>

        The observable weight of a peel-off photon packet will become numerically zero when the
        cumulative optical depth along its path is higher than \f$ \tau_\mathrm{max} =
        \ln(L/L_\mathrm{min}) \f$ where \f$L\f$ is the weight at the peel-off interaction site, and
        \f$L_\mathrm{min}\f$ is the smallest representable positive double-precision floating point
        number. Hence this function aborts the calculation and returns positive infinity when this
        happens. */
    double getOpticalDepth(PhotonPacket* pp, double distance) const;

    /** This function initializes all values of the primary and/or secondary radiation field info
        tables to zero. In simulation modes that record the radiation field, the function should be
        called before starting a simulation segment (i.e. before a set of photon packets is
        launched). If the \em primary flag is true, the function clears both the primary table and
        the stable secondary table (if present). The stable secondary table is cleared as well so
        that we can use its contents even if no secondary of photon packet segment has been
        launched yet. If the flag is false, the function clears just the temporary secondary table,
        so that the stable secondary table remains available for calculating secondary emission
        spectra. */
    void clearRadiationField(bool primary);

    /** This function adds the specified value of \f$L\,\Delta s\f$ to the radiation field bin
        corresponding to the spatial cell index \f$m\f$ and the wavelength index\f$\ell\f$. If the
        \em primary flag is true, the value is added to the primary table; otherwise it is added to
        the temporary secondary table.

        The addition happens in a thread-safe way, so that this function can be called from
        multiple parallel threads, even for the same spatial/wavelength bin. If any of the indices
        are out of range, undefined behavior results. */
    void storeRadiationField(bool primary, int m, int ell, double Lds);

    /** This function accumulates the radiation field between multiple processes. In simulation
        modes that record the radiation field, the function should be called in serial code after
        finishing a simulation segment (i.e. after a before set of photon packets has been
        launched) and before querying the radiation field's contents. If the \em primary flag is
        true, the primary table is synchronized; otherwise the temporary secondary table is
        synchronized and its contents is copied into the stable secondary table. */
    void communicateRadiationField(bool primary);

private:
    /** This function returns the sum of the values in both the primary and the stable secondary
        radiation field tables at the specified cell and wavelength indices. If a table is not
        present, the value for that table is assumed to be zero. */
    double radiationField(int m, int ell) const;

public:
    /** This function returns an array with the mean radiation field intensity
        \f$(J_\lambda)_{\ell,m}\f$ in the spatial cell with index \f$m\f$ at each of the wavelength
        bins \f$\ell\f$ defined by the wavelength grid returned by the
        Configuration::radiationFieldWLG() function.

        This function assumes that a set of photon packets have been launched for primary and/or
        secondary simulation segments, and that radiation field information has been accumulated
        during the life cycles by calling the storeRadiationField() function. Furthermore, the
        communicateRadiationField() function must have been called before invoking this function.
        If this is not the case, the behavior is undefined.

        The mean intensity is calculated using \f[ (J_\lambda)_{\ell,m} = \frac{ (L\Delta
        s)_{\ell,m} }{4\pi\,V_m\,(\Delta \lambda)_\ell} \f] where \f$\ell\f$ is the index of the
        wavelength bin, \f$(\Delta \lambda)_\ell\f$ is the wavelength bin width, \f$m\f$ is the
        spatial cell index, \f$V_m\f$ is the volume of the cell, and \f$(L\Delta s)_{\ell,m}\f$ has
        been accumulated over all photon packets contributing to the bin. The resulting mean
        intensity \f$J_\lambda\f$ is expressed as an amount of energy per unit of time, per unit of
        area, per unit of wavelength, and per unit of solid angle. */
    Array meanIntensity(int m) const;

    /** This function returns an indicative dust temperature for the spatial cell with index
        \f$m\f$. For each material mix of type dust present in the specified cell, the function
        calculates the equilibrium temperature that would be reached when the dust is embedded in
        the radiation field tracked by the simulation for the cell. This is achieved by solving the
        energy balance equation under LTE (local thermal equilibrium) assumptions, and using a
        single representative grain for the complete dust mix. The resulting temperatures are
        averaged over the dust components present in the spatial cell (weighed by relative mass in
        the cell). If the cell does not contain any dust, the function returns zero.

        In formula form, for a dust cell \f$m\f$ with dust components \f$h\f$, the indicative dust
        temperature is defined as \f[{\bar{T}}_m = \frac{\sum_h \rho_{m,h}\,{\bar{T}}_{m,h}}
        {\sum_h \rho_{m,h}} \f] where \f${\bar{T}}_{m,h}\f$ is the LTE equilibrium temperature for
        dust component \f$h\f$, obtained through the balance equation \f[ \int_0^\infty
        \varsigma_{h,\lambda}^{\text{abs}}\, J_{m,\lambda}\, {\text{d}}\lambda = \int_0^\infty
        \varsigma_{h,\lambda}^{\text{abs}}\, B_\lambda({\bar{T}}_{m,h})\, {\text{d}}\lambda. \f]

        Note that the indicative dust temperature does not correspond to a physical temperature.
        The LTE assumption is almost certainly unjustified for a relevant portion of the dust
        grains (depending on the embedding radiation field), and even when ignoring this problem,
        averaging temperatures over the dust components and over the various grain material types
        and grain sizes within a particular dust mix has no clear-cut physical justification nor
        interpretation. */
    double indicativeDustTemperature(int m) const;

    /** This function returns the bolometric luminosity \f$L^\text{abs}_{\text{bol},m}\f$ that has
        been absorbed by dust media in the spatial cell with index \f$m\f$.

        This function assumes that a set of photon packets have been launched for primary and/or
        secondary simulation segments, and that radiation field information has been accumulated
        during the life cycles by calling the storeRadiationField() function. Furthermore, the
        communicateRadiationField() function must have been called before invoking this function.
        If this is not the case, the behavior is undefined.

        The bolometric luminosity is calculated using \f[ L^\text{abs}_{\text{bol},m} = \sum_\ell
        (k^\text{abs}_\text{type})_{\ell,m} \,(L\Delta s)_{\ell,m} \f] where \f$\ell\f$ runs over
        the wavelengths in the simulation's radiation field wavelength grid, \f$m\f$ is the spatial
        cell index, \f$(k^\text{abs}_\text{dust})_{\ell,m}\f$ is the absorption opacity of the dust
        in the cell, and \f$(L\Delta s)_{\ell,m}\f$ has been accumulated over all photon packets
        contributing to the bin. */
    double absorbedDustLuminosity(int m) const;

    /** This function returns the bolometric luminosity absorbed by dust media across the complete
        domain of the spatial grid, using the partial radiation field stored in the table indicated
        by the \em primary flag (true for the primary table, false for the stable secondary table).
        The bolometric absorbed luminosity in each cell is calculated as described for the
        absorbedDustLuminosity() function. */
    double totalAbsorbedDustLuminosity(bool primary) const;

    /** This function returns an indicative gas temperature \f$T\f$ in the spatial cell with index
        \f$m\f$. This temperature is obtained by averaging the temperature over the gas medium
        components present in the spatial cell, weighed by relative mass in each component. If no
        medium component specifies a gas temperature, this function returns zero. */
    double indicativeGasTemperature(int m) const;

    //================== Private Types and Functions ====================

private:
    /** This data structure holds the information maintained per cell. */
    struct State1
    {
        double V{0.};  // volume
        Vec v;         // bulk velocity
        Vec B;         // magnetic field
        double T{0.};  // gas temperature
    };

    /** This data structure holds the information maintained per cell and per medium. */
    struct State2
    {
        double n{0.};                     // the number density
        //const MaterialMix* mix{nullptr};  // pointer to the material mix
    };

    /** This function returns a writable reference to the state data structure for the given cell
        index. */
    State1& state(int m) { return _state1v[m]; }

    /** This function returns a read-only reference to the state data structure for the given cell
        index. */
    const State1& state(int m) const { return _state1v[m]; }

    /** This function returns a writable reference to the state data structure for the given cell
        and medium indices. */
    State2& state(int m, int h) { return _state2vv[m * _numMedia + h]; }

    /** This function returns a read-only reference to the state data structure for the given cell
        and medium indices. */
    const State2& state(int m, int h) const { return _state2vv[m * _numMedia + h]; }

    /** This function communicates the cell states between multiple processes after the states have
        been initialized in parallel (i.e. each process initialized a subset of the states). */
    void communicateStates();

    //======================== Data Members ========================

private:
    Configuration* _config{nullptr};

    // relevant for any simulation mode that includes a medium
    int _numCells{0};          // index m
    int _numMedia{0};          // index h
    vector<State1> _state1v;   // state info for each cell (indexed on m)
    vector<State2> _state2vv;  // state info for each cell and each medium (indexed on m,h)
    vector<const MaterialMix*> _mixv;  // indexed on h, or on m and h if mixPerCell is true
    bool _mixPerCell{false};

    // relevant for any simulation mode that stores the radiation field
    WavelengthGrid* _wavelengthGrid{0};  // index ell
    // each radiation field table has an entry for each cell and each wavelength (indexed on m,ell)
    // - the sum of rf1 and rf2 represents the stable radiation field to be used as input for regular calculations
    // - rf2c serves as a target for storing the secondary radiation field so that rf1+rf2 remain available for
    //   calculating secondary emission spectra while already shooting photons through the grid
    Table<2> _rf1;   // radiation field from primary sources
    Table<2> _rf2;   // radiation field from secondary sources (copied from _rf2c at the appropriate time)
    Table<2> _rf2c;  // radiation field currently being accumulated from secondary sources
};

////////////////////////////////////////////////////////////////

#endif
