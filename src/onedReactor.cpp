#include <string>
#include <vector>
#include <memory>
#include <map>
#include <iostream>
#include <fstream>

#include <yaml-cpp/yaml.h>
#include "cantera/base/stringUtils.h"
#include "cantera/base/ct_defs.h"
#include "cantera/IdealGasMix.h"
#include "cantera/InterfaceLatInt.h"
#include "cantera/Interface.h"
#include "cantera/thermo/SurfLatIntPhase.h"
#include "cantera/thermo/SurfPhase.h"
#include "cantera/thermo/LateralInteraction.h"
#include "cantera/thermo/StoichSubstance.h"
#include "cantera/thermo/ThermoPhase.h"
#include "cantera/zeroD/Reactor.h"
#include "cantera/zeroD/ReactorNet.h"
#include "cantera/zeroD/Reservoir.h"
#include "cantera/zeroD/ReactorSurface.h"
#include "cantera/zeroD/flowControllers.h"

#include "util.h"
#include "reactor.h"
#include "pfr1d.h"
#include "pfr1d_solver.h"


namespace HeteroCt 
{

using namespace std;
using namespace Cantera;

void run_1d_reactor(YAML::Node& tube_node,
                    shared_ptr<IdealGasMix> gas, 
                    vector<shared_ptr<Interface>> surfaces,
                    ofstream& gen_info, 
                    bool transient_log)
{
    //Define the reactor based on the input file
    auto rctr_node = tube_node["reactor"];
    //auto rctr_type_node = rctr_node["type"];
    //auto rctr_type = rt[rctr_type_node.as<string>()];

    // Read the reactor dimensions

    auto area_node = rctr_node["area"];
    auto len_node = rctr_node["length"];
    //double rctr_vol = strSItoDbl(rctr_node["volume"].as<string>());
    if (!area_node || area_node.IsNull()){
        ;//TODO: Raise Error
    }
    if (!len_node || len_node.IsNull()){
        ;//TODO: Raise Error
    }
    double rctr_area = strSItoDbl(rctr_node["area"].as<string>());
    double rctr_len = strSItoDbl(rctr_node["length"].as<string>());

    double cat_abyv = strSItoDbl(rctr_node["cat_abyv"].as<string>());
    cout << "reached here" << endl;

    string mode = rctr_node["mode"].as<string>();
    /*if (mode == "isothermal") 
        rctr->setEnergy(0);
    else
        rctr->setEnergy(1);
    */

    cout << "reached here" << endl;
    auto inlet_node = tube_node["inlet_gas"];
    auto vel_node = inlet_node["velocity"];     //Units are len/s
    auto mfr_node = inlet_node["mass_flow_rate"];
    double velocity{0},  mfr{0};
    if (vel_node && !vel_node.IsNull()) {
        velocity = strSItoDbl(inlet_node["velocity"].as<string>());
        mfr = gas->density() * rctr_area * velocity;
    }
    else if (mfr_node && !mfr_node.IsNull()) {
        mfr = strSItoDbl(inlet_node["mass_flow_rate"].as<string>());
        velocity = mfr/ (gas->density() * rctr_area);
    }
    cout << "reached here2" << endl;

    vector<InterfaceKinetics*> ikin;
    vector<SurfPhase*> surf_ph;
    for (const auto surf: surfaces) {
        ikin.push_back(surf.get());
        surf_ph.push_back(surf.get());
    }

    // Start the simulation
    for (const auto surf: surfaces) {
        surf->solvePseudoSteadyStateProblem();
    }

    auto pfr = PFR1d(gas.get(), ikin, surf_ph, rctr_area, cat_abyv, velocity);
    vector<double> ydot(25);
    vector<double> y(25);
    pfr.getInitialConditions(0, y.data(), ydot.data());
    for (size_t i = 0; i < 25; i++){
        cout << "i:   " << i << "   y: " << y[i] << "   ydot:   " << ydot[i] << endl;
    }

    auto simul_node = tube_node["simulation"];
    //auto end_time = strSItoDbl(simul_node["end_time"].as<string>());
    auto solver_node = simul_node["solver"];
    auto abs_tol = solver_node["atol"].as<double>();
    auto rel_tol = solver_node["rtol"].as<double>();

    PFR1dSolver pfr_solver {&pfr};
    pfr_solver.setTolerances(rel_tol, abs_tol);
    pfr_solver.setMaxNumSteps(30000);  //TODO: Make this optional input option

    //pfr_solver(rctr_len);

    
    /*vector<double> zvals = get_log_intervals(rctr_len); //Use the same function to get z steps
    for (const auto& z : zvals) {
        pfr_solver.solve(z);
    }
    */
    pfr_solver.solve(rctr_len);
    pfr_solver.writeResults("1d_pfr.out");

    //vector<double> gas_X(gas->nSpecies());

    


   
}


}