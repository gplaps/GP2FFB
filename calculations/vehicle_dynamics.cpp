#define NOMINMAX
#include "vehicle_dynamics.h"
#include <ffb_setup.h>
#include <telemetry_reader.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// SAE Convention
// Lateral force 
// Negative <---------> Positive

// Left Turn
// Lateral G -> Positive
// Oversteer Slip -> Positive
// Understeer Slip -> Negative


// Vehicle Constants
// Most of this is estimates/guesses
// May or may not be used
namespace VehicleConstants {
    const double GRAVITY = 9.81; // m/s² (same for all)

    namespace GP2 {
        const double VEHICLE_MASS = 515.0 + 70.0 + 75.0; // kg (car + fuel + driver, 1994 regulations)
        const double FRONT_TRACK = 1.8; // m (typical 1994 F1)
        const double REAR_TRACK = 1.68; // m  
        const double WHEELBASE = 3.2; // m (typical 1994 F1)
        const double YAW_INERTIA = 900.0; // kg⋅m² (lighter than IndyCar)
        const double CG_FROM_FRONT = 1.44; // m (45% of wheelbase, F1 is more front-biased)
        const double CG_FROM_REAR = WHEELBASE - CG_FROM_FRONT; // m

        // Calculate scale from your observation: 4G = 460,000 raw units
        // 4G = 4 * 9.81 * 660kg = 25,924N total lateral force
        // Front wheels typically carry ~45% during cornering = ~11,666N
        // Scale = 11,666N / 460,000 raw units = 0.0254 N per raw unit
        const double TIRE_FORCE_SCALE = 0.0512; // Newtons per raw game unit ESTIMATE
    }
}

struct GameConstants {
    double VEHICLE_MASS;
    double FRONT_TRACK;
    double REAR_TRACK;
    double WHEELBASE;
    double YAW_INERTIA;
    double CG_FROM_FRONT;
    double CG_FROM_REAR;
    double TIRE_FORCE_SCALE;
    double MAX_GAME_FORCE_UNITS;
};

std::wstring GameToLower(const std::wstring& str) {
    std::wstring result = str;
    for (wchar_t& ch : result) ch = towlower(ch);
    return result;
}

// Select constants based on game

GameConstants GetGameConstants() {
    std::wstring gameVersionLower = GameToLower(targetGameVersion);

    if (gameVersionLower == L"x86gp2") {
        return {
            VehicleConstants::GP2::VEHICLE_MASS,
            VehicleConstants::GP2::FRONT_TRACK,
            VehicleConstants::GP2::REAR_TRACK,
            VehicleConstants::GP2::WHEELBASE,
            VehicleConstants::GP2::YAW_INERTIA,
            VehicleConstants::GP2::CG_FROM_FRONT,
            VehicleConstants::GP2::CG_FROM_REAR,
            VehicleConstants::GP2::TIRE_FORCE_SCALE,
        };
    }
    else {
        // Default to GP2 if unknown
        return {
            VehicleConstants::GP2::VEHICLE_MASS,
            VehicleConstants::GP2::FRONT_TRACK,
            VehicleConstants::GP2::REAR_TRACK,
            VehicleConstants::GP2::WHEELBASE,
            VehicleConstants::GP2::YAW_INERTIA,
            VehicleConstants::GP2::CG_FROM_FRONT,
            VehicleConstants::GP2::CG_FROM_REAR,
            VehicleConstants::GP2::TIRE_FORCE_SCALE,
        };
    }
}

// Helper function to convert raw tire data to usable data
double convertTireForceToNewtons(int32_t tire_force_raw) {
    // DON'T remove the sign - preserve it!
    GameConstants constants = GetGameConstants();

    double force_with_sign = static_cast<double>(tire_force_raw);
    return force_with_sign * constants.TIRE_FORCE_SCALE;
}

int getTurnDirection(int32_t lf, int32_t rf) {
    int negative_count = 0;
    if (lf < 0) negative_count++;
    if (rf < 0) negative_count++;

    if (negative_count >= 1) {
        return -1; // Left turn
    }
    else {
        return 1;  // Right turn
    }
}

bool CalculateVehicleDynamics(const RawTelemetry& current, RawTelemetry& previous, bool& firstReading, CalculatedVehicleDynamics& out) {
    if (firstReading) {
        previous = current;
        firstReading = false;
        return false;
    }

    GameConstants constants = GetGameConstants();

    // Convert units
    double speed_ms = current.gp2_speedKmh * 0.277778; // mph to m/s

    // Convert steering wheel angle to actual wheel angle
    // Typical steering ratio is around 12:1 to 15:1
    const double STEERING_RATIO = 15.0;
    double wheel_angle_deg = current.gp2_stWheelAngle / STEERING_RATIO;
    double wheel_angle_rad = wheel_angle_deg * M_PI / 180.0;

    
    // Force Assignments
    out.force_lf = static_cast<int32_t>(current.gp2_magLat_lf);  
    out.force_rf = static_cast<int32_t>(current.gp2_magLat_rf);
    //out.force_lr = static_cast<int16_t>(current.tiremaglat_lr);
    //out.force_rr = static_cast<int16_t>(current.tiremaglat_rr);
    out.forceLong_lf = static_cast<int32_t>(current.gp2_magLong_lf);
    out.forceLong_rf = static_cast<int32_t>(current.gp2_magLong_rf);
    //out.forceLong_lr = static_cast<int16_t>(current.tiremaglong_lr);
    //out.forceLong_rr = static_cast<int16_t>(current.tiremaglong_rr);

    // Convert tire forces to "actual" Newtons
    // In the future if we find real forces we can replace this
    double force_lf_N = convertTireForceToNewtons(out.force_lf);
    double force_rf_N = convertTireForceToNewtons(out.force_rf);
    //double force_lr_N = convertTireForceToNewtons(out.force_lr);
    //double force_rr_N = convertTireForceToNewtons(out.force_rr);
    double forceLong_lf_N = convertTireForceToNewtons(out.forceLong_lf);
    double forceLong_rf_N = convertTireForceToNewtons(out.forceLong_rf);
    //double forceLong_lr_N = convertTireForceToNewtons(out.forceLong_lr);
    //double forceLong_rr_N = convertTireForceToNewtons(out.forceLong_rr);

    

    out.frontLeftForce_N = convertTireForceToNewtons(out.force_lf);
    out.frontRightForce_N = convertTireForceToNewtons(out.force_rf);
    out.frontLeftLong_N = convertTireForceToNewtons(out.forceLong_lf);
    out.frontRightLong_N = convertTireForceToNewtons(out.forceLong_rf);

    
    // CALC 1
    //===== LATERAL FORCE CALC ======
    
    // Calculate sum of lateral force
    double total_lateral_force_N = force_lf_N + force_rf_N;

    // Determine turn direction and apply sign
    int turn_direction = getTurnDirection(out.force_lf, out.force_rf);

    // Calculate lateral acceleration: F = ma, so a = F/m
    double lateral_acceleration = total_lateral_force_N / constants.VEHICLE_MASS;

    // Calculate total lateral force (maybe unneeded)
    out.totalLateralForce = out.force_lf + out.force_rf;
    
    // Convert to G-force
    out.lateralG = lateral_acceleration / VehicleConstants::GRAVITY;

    // Calculate direction value
    if (std::abs(out.lateralG) < 0.05) {
        out.directionVal = 0; // Straight
    }
    else if (out.totalLateralForce > 0) {
        out.directionVal = 10000; // Left (positive totalLateralForce)
    }
    else {
        out.directionVal = -10000; // Right (negative totalLateralForce)
    }

    /*
    // CALC 2
    //===== SLIP ANGLE ======

    // This is to cut out random noise
    if (speed_ms > 2.0 && std::abs(out.lateralG) > 0.1) { // Only calculate when moving and turning

        // Calculate front and rear lateral forces
        double front_lateral_force = force_lf_N + force_rf_N;
        double rear_lateral_force = force_lr_N + force_rr_N;

        // Compare actual vs expected lateral acceleration
        double expected_lateral_accel = 0.0;
        if (std::abs(wheel_angle_rad) > 0.001) {
            // Expected lateral acceleration from steering input
            expected_lateral_accel = (speed_ms * speed_ms * std::tan(std::abs(wheel_angle_rad))) / constants.WHEELBASE;
        }

        double actual_lateral_accel = std::abs(out.lateralG) * VehicleConstants::GRAVITY;

        // Response ratio
        double response_ratio = 1.0;
        if (expected_lateral_accel > 0.1) {
            response_ratio = actual_lateral_accel / expected_lateral_accel;
        }

        // Front vs rear force balance
        double total_force = std::abs(front_lateral_force + rear_lateral_force);
        double force_balance = 0.0;
        if (total_force > 100.0) {
            // Positive = rear working harder (oversteer), Negative = front working harder (understeer)
            force_balance = (std::abs(rear_lateral_force) - std::abs(front_lateral_force)) / total_force;
        }

        // Method 3: Left/right tire imbalance (detects if one end is sliding)
        double front_imbalance = std::abs(std::abs(force_lf_N) - std::abs(force_rf_N)) / std::max(1.0, std::abs(force_lf_N + force_rf_N));
        double rear_imbalance = std::abs(std::abs(force_lr_N) - std::abs(force_rr_N)) / std::max(1.0, std::abs(force_lr_N + force_rr_N));

        // Higher imbalance = more sliding at that axle
        double axle_slip_difference = rear_imbalance - front_imbalance;

        // Combine methods to get slip indicator
        double slip_indicator = 0.0;

        // Response method: too much response = oversteer, too little = understeer
        // Tune this to make slip match reality
        if (response_ratio > 1.02) {
            slip_indicator += (response_ratio - 1.0) * 0.5; // Oversteer
        }
        else if (response_ratio < 0.98) {
            slip_indicator += (response_ratio - 1.0) * 0.5; // Understeer (negative)
        }

        // Force balance method
        slip_indicator += force_balance * 0.5;

        // Axle imbalance method  
        slip_indicator += axle_slip_difference * 0.3;

        // Convert to degrees and apply proper signs
        double slip_degrees = slip_indicator;

        // Apply turn-specific sign convention:
        // For LEFT turns: positive = oversteer, negative = understeer
        // For RIGHT turns: negative = oversteer, positive = understeer

        if (turn_direction > 0) { // Right turn
            slip_degrees = -slip_degrees; // Flip signs for right turns
        }
        // For left turns (turn_direction < 0), keep signs as calculated

        out.slip = slip_degrees;

    }
    else {
        out.slip = 0.0; // No slip when not moving or turning
    }
    */


    previous = current;
    return true;
}