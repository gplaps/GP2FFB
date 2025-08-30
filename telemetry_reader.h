// telemetry_reader.h
#include <string>
#pragma once

struct RawTelemetry {
    
    double gp2_structSize;
    
    double gp2_isInRace;
    double gp2_isPlayer;

    double gp2_isPaused;
    double gp2_isReplay;
    double gp2_isX86MenuOn;
    int gp2_deviceID;

    double gp2_speedKmh;
    double gp2_stWheelAngle;
    double gp2_tyreTurnAngle;
    double gp2_slipAngleFront;
    double gp2_slipAngleRear;

    float gp2_magLat_lf;
    float gp2_magLat_rf;

    float gp2_magLong_lf;
    float gp2_magLong_rf;

    double gp2_surfaceType_lf;
    double gp2_surfaceType_rf;
    double gp2_surfaceType_lr;
    double gp2_surfaceType_rr;

    double gp2_rideHeights_lf;
    double gp2_rideHeights_rf;
    double gp2_rideHeights_lr;
    double gp2_rideHeights_rr;

    double gp2_wheelSpin_13C_lf;
    double gp2_wheelSpin_13C_rf;
    double gp2_wheelSpin_13C_lr;
    double gp2_wheelSpin_13C_rr;

    double gp2_notOnDamper_lf;
    double gp2_notOnDamper_rf;
    double gp2_notOnDamper_lr;
    double gp2_notOnDamper_rr;

    double gp2_calc_248_lf;
    double gp2_calc_248_rf;
    double gp2_calc_248_lr;
    double gp2_calc_248_rr;

    double gp2_wheel_2AC_lf;
    double gp2_wheel_2AC_rf;
    double gp2_wheel_2AC_lr;
    double gp2_wheel_2AC_rr;

    bool valid = false;
};

// Include logging
void LogMessage(const std::wstring& msg);

// Returns true if data was read successfully
bool ReadTelemetryData(RawTelemetry& out);