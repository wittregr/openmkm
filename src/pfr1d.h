/**
 * @file pfr1d.h
 */

// Provides a 1d plug-flow reactor model.

// This file is part of OpenMKM. See License.txt in the top-level directory 
// for license and copyright information.

#ifndef OMKM_PFR_1D_H
#define OMKM_PFR_1D_H

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <memory>

#include <boost/math/interpolators/barycentric_rational.hpp>

#include "cantera/zeroD/ReactorBase.h"
#include "cantera/IdealGasMix.h"
#include "cantera/kinetics/InterfaceKinetics.h"
#include "cantera/thermo/SurfPhase.h"
#include "cantera/thermo/SurfLatIntPhase.h"
#include "cantera/numerics/eigen_dense.h"
#include "cantera/numerics/ResidEval.h"
#include "cantera/numerics/ResidJacEval.h"
#include "cantera/transport.h"

#ifndef SUPPRESS_WARNINGS
#define SUPPRESS_WARNINGS true
#endif

namespace OpenMKM
{

// Helper functions
static inline double circleArea(doublereal Di)
{
    return Cantera::Pi * Di * Di / 4;
}

static inline double sccmTocmps(doublereal sccm)
{
    return sccm / 60000000;
}

//! A plug flow reactor (PFR) model implemented in 1d. 
//!
/*!The model calculates the 
 * steady state conditions of the PFR as a function of z (axial direction).
 * To evaluate the steady state conditions, first a pseudo steady state is solved
 * for the surfaces at the inlet for given T and P conditions. The resulting 
 * state of PFR at the inlet is used to propagate the state as a function of z by
 * solving differential algebraic governing equations of the PFR.
 */

//TODO: Clean up PFR implementation to correspond to that of the
//TODO: zeroD reactors. Implement a Reactor Surface model 
//TODO: corresponding to zeroD 
//TODO: reactor surface model for the PFR. Current PFR doesn't 
//TODO: exhibit modularity well.
class PFR1d : public Cantera::ResidJacEval
{
public:
    //! Constructor
    /*!
     * @param gas Gas phase (IdealGasMix object) containing both thermo 
     *            properties and kinetics manager to initialize the class.
     * @param surf_kins Kinetics managers of the surfaces. These have to 
     *                  instances of either Interface or InterfaceInteractions 
     *                  classes. Supply them as vector<InterfaceKinetics*>
     * @param surf_phases Surface phases corresponding to the interface kinetics
     *                    managers. These have to instances of either Interface 
     *                    or InterfaceInteractions classes. Supply the same 
     *                    objects used for surf_kins. Supply them as 
     *                    vector<SurfPhase*>.
     * @param pfr_xc_area PFR Cross section area
     * @param cat_abyv    Catalyst area given w.r.t. pfr volume
     * @param gas_velocity  Velocity of the gas entering the reactor
     *                    Convert flow rates, residence times and mass flow rates
     *                    into velocity using the reactor size and density
     */
    PFR1d(Cantera::IdealGasMix *gas, 
          std::vector<Cantera::InterfaceKinetics*> surf_kins,       
          std::vector<Cantera::SurfPhase*> surf_phases,
          double pfr_xc_area,
          double cat_abyv, 
          double gas_flowrate);

    ~PFR1d()
    {
        Cantera::appdelete();
        //m_T_interp = nullptr;
    }

    void reinit();

    virtual int getInitialConditions(const double t0,
                                     double *const y,
                                     double *const ydot);

    //! Evalueate the residual functional F(z, y, y') = 0 of differential 
    //! algebraic equations corresponding to 1d PFR.
    /*!
     * @param t z value from the PFR entrance. In DAE parlance, this is the
     *          individual variable and is typically denoted as t, because 
     *          often time is the independent variable. 
     * @param delta_t Step in z to evaluate the jacobian
     * @param y State of the PFR. The state consists of gas velocity, density,
     *          pressure, gas mass fractions and coverages of the surfaces.
     * @param ydot First order derivates of the state variables w.r.t. z.
     * @param resid Residual function F(t, y, y') corresponding to the DAEs 
     *              of PFR
     * @param evalType
     */
    virtual int evalResidNJ(const double t,
                    const double delta_t,
                    const double* const y,
                    const double* const ydot,
                    double* const resid,
                    const Cantera::ResidEval_Type_Enum evalType = Cantera::Base_ResidEval,
                    const int id_x = -1,
                    const double delta_x = 0.0);

    //! Evaluate the production rates of the species at the surfaces
    //! Returns the total mass of species produced at surfaces
    double evalSurfaces();

    //! Evalueate the quadrature with integrand ROP(z, y, y') of differential 
    //! algebraic equations corresponding to 1d PFR.
    /*!
     * @param t z value from the PFR entrance. In DAE parlance, this is the
     *          individual variable and is typically denoted as t, because 
     *          often time is the independent variable. 
     * @param y State of the PFR. The state consists of gas velocity, density,
     *          pressure, gas mass fractions and coverages of the surfaces.
     * @param ydot First order derivates of the state variables w.r.t. z.
     * @param rhsQ Integrand function F(t, y, y') corresponding to the rate of progress 
     *              of the reactions 
     * @param evalType
     */
    virtual int evalQuadRhs(const double t,
                    const double* const y,
                    const double* const ydot,
                    double* const rshQ);

    unsigned getSpeciesIndex(std::string name) const
    {
        return m_gas->kineticsSpeciesIndex(name);
    }

    //! Setup the constraints for each of the governing the numerical solver
    void setConstraints();

    //! Get the internal energy per unit mass of the fluid in the reactor
    double getIntEnergyMass() const
    {
        return m_gas->intEnergy_mass();
    }

    std::vector<std::string> variablesNames() const
    {
        return m_var;
    }

    std::vector<std::string> stateVariableNames() const
    {
        std::vector<std::string> state_var;
        for (size_t i = 0; i < m_neqs_extra; i++)
            state_var.push_back(m_var[i]);
        return state_var;
    }

    std::vector<std::string> gasVariableNames() const
    {
        std::vector<std::string> gas_var;
        for (size_t i = 0; i < m_nsp; i++){
            auto k = i + m_neqs_extra;
            gas_var.push_back(m_var[k]);
        }
        return gas_var;
    }

    std::vector<std::string> surfaceVariableNames()
    {
        size_t nsurf = neq_ - m_neqs_extra - m_nsp;
        std::vector<std::string> surf_var;
        for (size_t i = 0; i < nsurf; i++){
            auto k = i + m_neqs_extra + m_nsp;
            surf_var.push_back(m_var[k]);
        }
        return surf_var;
    }

    //! Set the fluid (gas) flowrate 
    //! The code internally uses velocity as state variable
    void setFlowRate(double flow_rate) 
    {
        if (!m_Ac){
            auto velocity = flow_rate / m_Ac;
            m_u0 = velocity;
        }
        else {
            throw Cantera::CanteraError("PFR1d::setFlowRate",
                               "Reactor cross section not defined.");
        }
    }

    //! Set the fluid (gas) velocity 
    void setVelocity(double velocity) 
    {
        m_u0 = velocity;
    }

    void getSurfaceInitialConditions(double* y);

    void getSurfaceProductionRates(double* y);

    //! Enable energy balance condition to be solved
    //! To be used with adiabatic and heat transfer modes
    void setEnergy(int eflag) 
    {
        if (eflag > 0) {
            m_energy = true;
            m_neqs_extra = 4;
        } else {
            m_energy = false;
            m_neqs_extra = 3;
        }
    }

    //! Checks whether energy balance condition is enabled
    bool energyEnabled() const 
    {
        return m_energy;
    }

    //! Setup heat transfer from external source. 
    //! which is transferring heat through 
    //! wall_abyv and heat transfer coefficient htc
    /*!
     * @param htc   Heat transfer coefficient of the conducting wall 
     * @param Text  Temperature of exteranl heat source
     * @param wall_abyv Area of the condicting wall with 
     */
    void setHeatTransfer(double htc, double Text, double wall_abyv);

    //! Compute the amount of heat transferred from all external heat sources. 
    /*!
     * @param Tint  Temperature of the reactor 
     */
    double getHeat(double Tint) const;

    //! User supplied T profile in the PFR.
    //! The profile is supplied as map of distance and temperature values.
    /*!
     * If the first z is 0.0, then T has to be equal to m_T0 at inlet.
     * The code doesn't do the check and if the condition is not hold,
     * the behavior is undefined.
     * @param T_profile Temperature profile of the PFR as a function of 
     *                  distance from inlet
     */
    void setTProfile(const std::map<double, double>& T_profile);

    //! Return the temperature of the reactor at distance z from inlet.
    //! Applicable only for cases where energy equation is not solved.
    /*!
     * @param z: Distance from inlet (in m)
     */
    double getT(double z);

    Cantera::thermo_t& contents() 
    {
        if (!m_gas) {
            throw Cantera::CanteraError("PFR1d::contents",
                               "Reactor contents not defined.");
        }
        return *m_gas;
    }

    const Cantera::thermo_t& contents() const 
    {
        if (!m_gas) {
            throw Cantera::CanteraError("PFR1d::contents",
                               "Reactor contents not defined.");
        }
        return *m_gas;
    }

    //! Return a reference to the *n*-th SurfPhase connected to the PFR
    Cantera::SurfPhase* surface(size_t n) 
    {
        return m_surf_phases[n];
    }

    //! Number of sensitivity parameters associated with this reactor
    //! (including walls)
    virtual size_t nSensParams();

    //! Add a sensitivity parameter associated with the reaction number *rxn*
    //! (in the homogeneous phase).
    void addSensitivityReaction(std::string& rxn_id);

    //! Add a sensitivity parameter associated with the enthalpy formation of
    //! species *species_name* 
    void addSensitivitySpecies(std::string& species_name);

protected:
    //! Pointer to the gas phase object.
    Cantera::IdealGasMix *m_gas = nullptr;

    //! Surface kinetics objects
    std::vector<Cantera::InterfaceKinetics*> m_surf_kins;

    void addSensitivityReaction(size_t kin_ind, size_t rxn_id);

    //! Add a sensitivity parameter associated with the enthalpy formation of
    //! species *k* in the kth phase 
    //! (gas phase index at 0, surf phase index starts at 1)
    void addSensitivitySpeciesEnthalpy(size_t phase_ind, size_t k);

    //! Surface phases objects.
    //! Both surface kinetics and phases have to refer
    //! to same objects, which are instances of ether
    //! Interface or InterfaceInteractions classes
    std::vector<Cantera::SurfPhase*> m_surf_phases;

    //! Species molar weights.
    std::vector<doublereal> m_W;

    //! Species net production rates in  gas phase reactions.
    std::vector<double> m_wdot;

    //! Species net production rates in surface reactions.
    std::vector<double> m_sdot;

    //! Species names and variables.
    std::vector<std::string> m_var;

    //! Number of equations in residual.
    //unsigned m_neq;

    //! Number of gas species.
    unsigned m_nsp;

    //! Number of extra equations to solve beyond gas and surface species number
    unsigned m_neqs_extra; 

    //! Catalyst area by volume
    double m_cat_abyv;

    //! Reference state inlet density.
    double m_rho_ref;

    //! Reactor cross-sectional area
    double m_Ac; 

    //! Solve Energy Equation
    bool m_energy = 0;

    //! Inlet gas velocity
    double m_u0;// = 0.0;

    //! Inlet temperature
    double m_T0 = 0.0;

    //! Tprofile
    // The idea is to use m_T_profile_iind to keep track of current index of 
    // T_profile. For distance between m_T_profile_ind[m_T_profile_iind] and 
    // m_T_profile_ind[m_T_profile_iind+1], T is given as linear interpolation 
    // of m_T_profile[m_T_profile_iind] and m_T_profile[m_T_profile_iind+1].
    // If m_T_profile_ind[0] > 0.0, then m_T_profile_iind=-1 and T is given as
    // linear interpolation of m_T0 and m_T_profile[0]
    //! Not used anymore. Will be deleted in future release
    std::vector<double> m_T_profile;

    //! Index of Tprofile in terms of distance
    //! Not used anymore. Will be deleted in future release
    std::vector<double> m_T_profile_ind;
    
    //! Current index of Tprofile_ind
    //! Not used anymore. Will be deleted in future release
    //int m_T_profile_iind;

    //! Barycentric interpolator to interpolate temperatures for T profile 
    std::shared_ptr<boost::math::barycentric_rational<double>> m_T_interp;

    //! External Heat supplied 
    bool m_heat = false;

    //! External temperature
    double m_Text = 0;

    //! Heat transfer coefficent
    double m_htc = 0;

    //! Reactor Wall area where heat is transferred
    double m_surf_ext_abyv = 0;

    //! Inlet pressure
    double m_P0 = 0;

    //! Set reaction rate multipliers based on the sensitivity variables in
    //! *params*.
    void applySensitivity();
    //! Reset the reaction rate multipliers
    void resetSensitivity();

    //! Data associated each sensitivity parameter
    std::vector<std::vector<Cantera::SensitivityParameter> > m_sensParams;

    //! Names corresponding to each sensitivity parameter
    std::vector<std::string> m_paramNames;
}; 

} 
#endif 
