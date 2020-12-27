/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "vehicles.h"

#include <osp/Satellites/SatVehicle.h>

using namespace testapp;

using osp::Vector2;
using osp::Vector3;
using osp::Matrix4;
using osp::Quaternion;

using osp::Package;
using osp::DependRes;

using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::SatVehicle;
using osp::universe::UCompTransformTraj;
using osp::universe::UCompVehicle;

using osp::BlueprintVehicle;
using osp::PrototypePart;


osp::universe::Satellite testapp::debug_add_deterministic_vehicle(
        Universe& uni, Package& pkg, std::string const & name)
{
    // Begin blueprint
    BlueprintVehicle blueprint;

    // Part to add
    DependRes<PrototypePart> rocket = pkg.get<PrototypePart>("part_stomper");
    blueprint.add_part(rocket, Vector3(0.0f), Quaternion(), Vector3(1.0f));

    // Wire throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.add_wire(0, 0, 1,
        0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MachineUserControl m_woAttitude
    // to    (input): a MachineRocket m_wiGimbal
    blueprint.add_wire(0, 0, 0,
        0, 1, 0);

    // Save blueprint
    DependRes<BlueprintVehicle> depend = pkg
            .add<BlueprintVehicle>(name, std::move(blueprint));

    // Create new satellite
    Satellite sat = uni.sat_create();

    // Set name
    auto& posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_name = name;

    // Make it into a vehicle
    auto& typeVehicle = *static_cast<SatVehicle*>(
            uni.sat_type_find("Vehicle")->second.get());

    UCompVehicle &vehicle = typeVehicle.add_get_ucomp(sat);
    vehicle.m_blueprint = std::move(depend);

    return sat;
}

osp::universe::Satellite testapp::debug_add_random_vehicle(
        osp::universe::Universe& uni, osp::Package& pkg,
        std::string const & name)
{

    // Start making the blueprint
    BlueprintVehicle blueprint;

    // Part to add, very likely a spamcan
    DependRes<PrototypePart> victim = pkg.get<PrototypePart>("part_spamcan");

    // Add 12 parts
    for (int i = 0; i < 12; i ++)
    {
        // Generate random vector
        Vector3 randomvec(std::rand() % 64 - 32,
                          std::rand() % 64 - 32,
                          (i - 6) * 12);

        randomvec /= 64.0f;

        // Add a new [victim] part
        blueprint.add_part(victim, randomvec,
                           Quaternion(), Vector3(1, 1, 1));
        //std::cout << "random: " <<  << "\n";
    }

    // Wire throttle control
    // from (output): a MachineUserControl m_woThrottle
    // to    (input): a MachineRocket m_wiThrottle
    blueprint.add_wire(0, 0, 1,
                       0, 1, 2);

    // Wire attitude control to gimbal
    // from (output): a MachineUserControl m_woAttitude
    // to    (input): a MachineRocket m_wiGimbal
    blueprint.add_wire(0, 0, 0,
                       0, 1, 0);

    // put blueprint in package
    DependRes<BlueprintVehicle> depend = pkg.add<BlueprintVehicle>(name,
                                                        std::move(blueprint));

    // Create the Satellite containing a SatVehicle

    // Create blank satellite
    Satellite sat = uni.sat_create();

    // Set the name
    auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_name = name;

    // Make it into a vehicle
    auto &typeVehicle = *static_cast<SatVehicle*>(
            uni.sat_type_find("Vehicle")->second.get());
    UCompVehicle &UCompVehicle = typeVehicle.add_get_ucomp(sat);

    // set the SatVehicle's blueprint to the one just made
    UCompVehicle.m_blueprint = std::move(depend);

    return sat;

}