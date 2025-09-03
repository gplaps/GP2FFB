#pragma once

#include <dinput.h>
#include "../telemetry_reader.h"


void ApplyPeriodicVibrationEffect(const RawTelemetry& current,
    IDirectInputEffect* periodicVibrationEffect,
    bool enableVibrationForce,
    double masterForceScale,
    double vibrationForceScale);

// Function to create the periodic vibration effect
HRESULT CreatePeriodicVibrationEffect(IDirectInputDevice8* device, IDirectInputEffect** periodicVibrationEffect);

// Function to cleanup periodic effects
void CleanupPeriodicEffect(IDirectInputEffect* periodicVibrationEffect);