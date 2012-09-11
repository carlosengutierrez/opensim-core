/* -------------------------------------------------------------------------- *
 *            OpenSim:  MuscleMetabolicPowerProbeUmberger2003.cpp             *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2012 Stanford University and the Authors                *
 * Author(s): Tim Dorn                                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied    *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */


//=============================================================================
// INCLUDES and STATICS
//=============================================================================
#include "MuscleMetabolicPowerProbeUmberger2003.h"


using namespace std;
using namespace SimTK;
using namespace OpenSim;


//=============================================================================
// CONSTRUCTOR(S) AND SETUP
//=============================================================================
//_____________________________________________________________________________
/**
 * Default constructor.
 */
MuscleMetabolicPowerProbeUmberger2003::MuscleMetabolicPowerProbeUmberger2003() : Probe()
{
    setNull();
    constructProperties();
}

//_____________________________________________________________________________
/** 
 * Convenience constructor
 */
MuscleMetabolicPowerProbeUmberger2003::MuscleMetabolicPowerProbeUmberger2003(bool activation_maintenance_rate_on, 
    bool shortening_rate_on, bool basal_rate_on, bool work_rate_on) : Probe()
{
    setNull();
    constructProperties();

    set_activation_maintenance_rate_on(activation_maintenance_rate_on);
    set_shortening_rate_on(shortening_rate_on);
    set_basal_rate_on(basal_rate_on);
    set_mechanical_work_rate_on(work_rate_on);
}


//_____________________________________________________________________________
/**
 * Set the data members of this MuscleMetabolicPowerProbeUmberger2003 to their null values.
 */
void MuscleMetabolicPowerProbeUmberger2003::setNull()
{
    // no data members
}

//_____________________________________________________________________________
/**
 * Connect properties to local pointers.
 */
void MuscleMetabolicPowerProbeUmberger2003::constructProperties()
{
    constructProperty_activation_maintenance_rate_on(true);
    constructProperty_shortening_rate_on(true);
    constructProperty_basal_rate_on(true);
    constructProperty_mechanical_work_rate_on(true);

    constructProperty_scaling_factor(1.0);
    constructProperty_basal_coefficient(1.51);
    constructProperty_basal_exponent(1.0);
    constructProperty_MetabolicMuscleParameterSet(MetabolicMuscleParameterSet());
}



//=============================================================================
// MODEL COMPONENT METHODS
//=============================================================================
//_____________________________________________________________________________
/**
 * Perform some set up functions that happen after the
 * object has been deserialized or copied.
 *
 * @param aModel OpenSim model containing this MuscleMetabolicPowerProbeUmberger2003.
 */
void MuscleMetabolicPowerProbeUmberger2003::connectToModel(Model& aModel)
{
    Super::connectToModel(aModel);
}



//=============================================================================
// COMPUTATION
//=============================================================================
//_____________________________________________________________________________
/**
 * Compute muscle metabolic power.
 * Units = W/kg.
 * Note: for muscle velocities, Vm, we define Vm<0 as shortening and Vm>0 as lengthening.
 */
SimTK::Vector MuscleMetabolicPowerProbeUmberger2003::computeProbeInputs(const State& s) const
{
    // Initialize metabolic energy rate values
    double AMdot, Sdot, Bdot, Wdot;
    AMdot = Sdot = Bdot = Wdot = 0;

    // BASAL METABOLIC RATE for whole body
    // ------------------------------------------
    if (get_basal_rate_on() == true)
    {
        Bdot = get_basal_coefficient() * pow(_model->getMatterSubsystem().calcSystemMass(s), get_basal_exponent());
        if (Bdot == NaN)
            cout << "WARNING::" << getName() << ": Bdot = NaN!" << endl;
    }
    

    // Loop through each muscle in the MetabolicMuscleParameterSet
    int nM = get_MetabolicMuscleParameterSet().getSize();
    Vector Edot(nM);
    for (int i=0; i<nM; i++)
    {
        // Get a pointer to the current muscle in the model
        MetabolicMuscleParameter mm = get_MetabolicMuscleParameterSet().get(i);
        Muscle* m = checkValidMetabolicMuscle(mm);

        // Get some muscle properties at the current time state
        double max_isometric_force = m->getMaxIsometricForce();
        double max_shortening_velocity = m->getMaxContractionVelocity();
        double activation = m->getActivation(s);
        double excitation = m->getControl(s);
        double fiber_force_passive = m->getPassiveFiberForce(s);
        double fiber_force_active = m->getActiveFiberForce(s);
        double fiber_force_total = m->getFiberForce(s);
        double fiber_length_normalized = m->getNormalizedFiberLength(s);
        double fiber_velocity = m->getFiberVelocity(s);
        double fiber_velocity_normalized = m->getNormalizedFiberVelocity(s);
        double slow_twitch_excitation = mm.getRatioSlowTwitchFibers() * sin(Pi/2 * excitation);
        double fast_twitch_excitation = (1 - mm.getRatioSlowTwitchFibers()) * (1 - cos(Pi/2 * excitation));
        double A, A_rel, B_rel;

        // Set activation dependence scaling parameter: A
        if (excitation > activation)
            A = excitation;
        else
            A = (excitation + activation) / 2;

        // Get the normalized active fiber force, F_iso, that 'would' be developed at the current activation
        // and fiber length under isometric conditions (i.e. Vm=0)
        if (max_isometric_force == 0) {
            stringstream errorMessage;
            errorMessage << "Error: Max isometric force for muscle ' " 
                << m->getName() << "' is zero." << endl;
            throw (Exception(errorMessage.str()));
        }

        if (m->getForceVelocityMultiplier(s) == 0) {
            stringstream errorMessage;
            errorMessage << "Error: Force-velocity multiplier for muscle ' " 
                << m->getName() << "' is zero." << endl;
            throw (Exception(errorMessage.str()));
        }

        double F_iso = (fiber_force_active/m->getForceVelocityMultiplier(s)) / max_isometric_force;

        // DEBUG
        //cout << "fiber_velocity_normalized = " << fiber_velocity_normalized << endl;
        //cout << "fiber_velocity_multiplier = " << m->getForceVelocityMultiplier(s) << endl;
        //cout << "fiber_force_active = " << fiber_force_active << endl;
        //cout << "fiber_force_total = " << fiber_force_total << endl;
        //cout << "max_isometric_force = " << max_isometric_force << endl;
        //cout << "F_iso = " << F_iso << endl;
        //system("pause");


        // Set normalized hill constants: A_rel and B_rel
        A_rel = 0.1 + 0.4*(1 - mm.getRatioSlowTwitchFibers());
        B_rel = A_rel * max_shortening_velocity;

        // Warnings
        if (fiber_length_normalized < 0)
            cout << "WARNING: (t = " << s.getTime() 
            << "), muscle '" << m->getName() 
            << "' has negative normalized fiber-length." << endl; 



        // ACTIVATION & MAINTENANCE HEAT RATE for muscle i
        // --> depends on the normalized fiber length of the contractile element
        // -----------------------------------------------------------------------
        if (get_activation_maintenance_rate_on())
        {
            double unscaledAMdot = 128*(1 - mm.getRatioSlowTwitchFibers()) + 25;

            if (fiber_length_normalized <= 1.0)
                AMdot = get_scaling_factor() * std::pow(A, 0.6) * unscaledAMdot;
            else
                AMdot = get_scaling_factor() * std::pow(A, 0.6) * ((0.4 * unscaledAMdot) + (0.6 * unscaledAMdot * F_iso));
        }



        // SHORTENING HEAT RATE for muscle i
        // --> depends on the normalized fiber length of the contractile element
        // --> note that we define Vm<0 as shortening and Vm>0 as lengthening
        // -----------------------------------------------------------------------
        if (get_shortening_rate_on())
        {
            double Vmax_slowtwitch = max_shortening_velocity / (1 + 1.5*(1 - mm.getRatioSlowTwitchFibers()));
            double Vmax_fasttwitch = 2.5*Vmax_slowtwitch;
            double alpha_shortening_fasttwitch = 153 / Vmax_fasttwitch;
            double alpha_shortening_slowtwitch = 100 / Vmax_slowtwitch;
            double unscaledSdot, tmp_slowTwitch, tmp_fastTwitch;

            // Calculate UNSCALED heat rate --- muscle velocity dependent
            // -----------------------------------------------------------
            if (fiber_velocity_normalized <= 0)    // concentric contraction, Vm<0
            {
                const double maxShorteningRate = 100;

                tmp_slowTwitch = alpha_shortening_slowtwitch * fiber_velocity_normalized * mm.getRatioSlowTwitchFibers();
                if (tmp_slowTwitch > maxShorteningRate) {
                    cout << "WARNING: " << getName() << "  (t = " << s.getTime() << 
                        "Slow twitch shortening heat rate exceeds the max value of " << maxShorteningRate << 
                        " W/kg. Setting to " << maxShorteningRate << " W/kg." << endl; 
                    tmp_slowTwitch = maxShorteningRate;		// limit maximum value to 100 W.kg-1
                }

                tmp_fastTwitch = alpha_shortening_fasttwitch * fiber_velocity_normalized * (1-mm.getRatioSlowTwitchFibers());
                if (tmp_fastTwitch > maxShorteningRate) {
                    cout << "WARNING: " << getName() << "  (t = " << s.getTime() << 
                        "Fast twitch shortening heat rate exceeds the max value of " << maxShorteningRate << 
                        " W/kg. Setting to " << maxShorteningRate << " W/kg." << endl; 
                    tmp_fastTwitch = maxShorteningRate;		// limit maximum value to 100 W.kg-1
                }

                unscaledSdot = -tmp_slowTwitch - tmp_fastTwitch;                               // unscaled shortening heat rate: muscle shortening
            }

            else	// eccentric contraction, Vm>0
                unscaledSdot = -0.3 * alpha_shortening_slowtwitch * fiber_velocity_normalized;  // unscaled shortening heat rate: muscle lengthening


            // Calculate SCALED heat rate --- muscle velocity and length dependent
            // ---------------------------------------------------------------------
            if (fiber_velocity_normalized <= 0)     // concentric contraction, Vm<0
                Sdot = get_scaling_factor() * std::pow(A, 2.0) * unscaledSdot;
            else                                    // eccentric contraction,  Vm>0
                Sdot = get_scaling_factor() * A * unscaledSdot;

            if (fiber_length_normalized > 1.0)
                Sdot *= F_iso;  
        }
        


        // MECHANICAL WORK RATE for muscle i
        // --> note that we define Vm<0 as shortening and Vm>0 as lengthening
        // ------------------------------------------
        if (get_mechanical_work_rate_on())
        {
            if (fiber_velocity <= 0)    // concentric contraction, Vm<0
                Wdot = -fiber_force_active*fiber_velocity;
            else						// eccentric contraction, Vm>0
                Wdot = 0;

            Wdot /= mm.getMuscleMass();
        }


        // NAN CHECKING
        // ------------------------------------------
        if (AMdot == NaN)
            cout << "WARNING::" << getName() << ": AMdot (" << m->getName() << ") = NaN!" << endl;
        if (Sdot == NaN)
            cout << "WARNING::" << getName() << ": Sdot (" << m->getName() << ") = NaN!" << endl;
        if (Wdot == NaN)
            cout << "WARNING::" << getName() << ": Wdot (" << m->getName() << ") = NaN!" << endl;




        // TOTAL METABOLIC ENERGY RATE for muscle i
        // ------------------------------------------
        Edot(i) = AMdot + Sdot + Wdot;
        

        // DEBUG
        // ----------
        bool debug = false;
        if(debug) {
            cout << "muscle_mass = " << mm.getMuscleMass() << endl;
            cout << "ratio_slow_twitch_fibers = " << mm.getRatioSlowTwitchFibers() << endl;
            cout << "bodymass = " << _model->getMatterSubsystem().calcSystemMass(s) << endl;
            cout << "max_isometric_force = " << max_isometric_force << endl;
            cout << "activation = " << activation << endl;
            cout << "excitation = " << excitation << endl;
            cout << "fiber_force_total = " << fiber_force_total << endl;
            cout << "fiber_force_active = " << fiber_force_active << endl;
            cout << "fiber_length_normalized = " << fiber_length_normalized << endl;
            cout << "fiber_velocity = " << fiber_velocity << endl;
            cout << "slow_twitch_excitation = " << slow_twitch_excitation << endl;
            cout << "fast_twitch_excitation = " << fast_twitch_excitation << endl;
            cout << "max shortening velocity = " << max_shortening_velocity << endl;
            cout << "A_rel = " << A_rel << endl;
            cout << "B_rel = " << B_rel << endl;
            cout << "AMdot = " << AMdot << endl;
            cout << "Sdot = " << Sdot << endl;
            cout << "Bdot = " << Bdot << endl;
            cout << "Wdot = " << Wdot << endl;
            cout << "Edot = " << Edot(i) << endl;
            int res = system("pause");
        }
    }

    SimTK::Vector EdotTotal(1, Edot.sum() + Bdot);


    // This check is from Umberger(2003), page 104, but it will be ignored
    // for now as it is unclear whether it refers to a single muscle, or the
    // total of all muscles in the model.
    // -----------------------------------------------------------------------
    //if(EdotTotal(0) < 1.0 
    //    && get_activation_maintenance_rate_on() 
    //    && get_shortening_rate_on() 
    //    && get_mechanical_work_rate_on()) {
    //        cout << "WARNING: " << getName() 
    //            << "  (t = " << s.getTime() 
    //            << "), the model has a net metabolic energy rate of less than 1.0 W.kg-1." << endl; 
    //        EdotTotal(0) = 1.0;			// not allowed to fall below 1.0 W.kg-1
    //}

    return EdotTotal;
}


//_____________________________________________________________________________
/** 
 * Returns the number of probe inputs in the vector returned by computeProbeInputs().
 */
int MuscleMetabolicPowerProbeUmberger2003::getNumProbeInputs() const
{
    return 1;
}


//_____________________________________________________________________________
/** 
 * Provide labels for the probe values being reported.
 */
Array<string> MuscleMetabolicPowerProbeUmberger2003::getProbeOutputLabels() const 
{
    Array<string> labels;
    labels.append(getName());
    return labels;
}


//_____________________________________________________________________________
/**
 * Check that the MetabolicMuscleParameter is a valid object.
 * If all tests pass, a pointer to the muscle in the model is returned.
 *
 * @param mm MetabolicMuscleParameter object to check
 * @return *musc Muscle object in model
 */
Muscle* MuscleMetabolicPowerProbeUmberger2003::checkValidMetabolicMuscle(MetabolicMuscleParameter mm) const
{
    stringstream errorMessage;
    Muscle* musc;

    // check that the muscle exists
    int k = _model->getMuscles().getIndex(mm.getName());
    if( k < 0 )	{
        errorMessage << "MetabolicMuscleParameter: Invalid muscle '" 
            << mm.getName() << "' specified." << endl;
        throw (Exception(errorMessage.str()));
    }
    else {
        musc = &_model->updMuscles().get(k);
    }

    // error checking: muscle_mass
    if (mm.getMuscleMass() <= 0) {
        errorMessage << "MetabolicMuscleParameter: Invalid muscle_mass for muscle: " 
            << mm.getName() << ". muscle_mass must be positive." << endl;
        throw (Exception(errorMessage.str()));
    }

    // error checking: ratio_slow_twitch_fibers
    if (mm.getRatioSlowTwitchFibers() < 0 || mm.getRatioSlowTwitchFibers() > 1)	{
        errorMessage << "MetabolicMuscleParameter: Invalid ratio_slow_twitch_fibers for muscle: " 
            << mm.getName() << ". ratio_slow_twitch_fibers must be between 0 and 1." << endl;
        throw (Exception(errorMessage.str()));
    }

    //cout << "VALID muscle: " << mm.getName() << endl;
    return musc;
}