#include "periodic_force.h"
#include <iostream>

/*
 * Copyright 2025 gplaps
 *
 * Licensed under the MIT License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/MIT
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

 // External logging function
extern void LogMessage(const std::wstring& msg);

void ApplyPeriodicVibrationEffect(const RawTelemetry& current,
    IDirectInputEffect* periodicVibrationEffect,
    bool enableVibrationForce,
    double masterForceScale,
    double vibrationForceScale) {

    // Debug logging every 60 frames
    static int debugCounter = 0;
    debugCounter++;

    if (debugCounter % 300 == 0) {  // Every 5 seconds instead of every 1 second
        LogMessage(L"[VIBRATION DEBUG] Speed: " + std::to_wstring(current.gp2_speedKmh) +
            L", Surface LF: " + std::to_wstring(current.gp2_surfaceType_lf) +
            L", Surface RF: " + std::to_wstring(current.gp2_surfaceType_rf) +
            L", Enable: " + std::to_wstring(enableVibrationForce) +
            L", VibScale: " + std::to_wstring(vibrationForceScale) +
            L", Effect ptr: " + std::to_wstring(periodicVibrationEffect != nullptr));
    }

    if (!periodicVibrationEffect || !enableVibrationForce) {
        if (debugCounter % 600 == 0) {  // Every 10 seconds instead of every 2 seconds
            LogMessage(L"[VIBRATION DEBUG] Effect disabled - ptr: " + std::to_wstring(periodicVibrationEffect != nullptr) +
                L", enabled: " + std::to_wstring(enableVibrationForce));
        }
        return;
    }

    // Check if any tire is on kerb (surface types 1 or 2)
    bool onKerb = (current.gp2_surfaceType_lf == 1 || current.gp2_surfaceType_lf == 2 ||
        current.gp2_surfaceType_rf == 1 || current.gp2_surfaceType_rf == 2 ||
        current.gp2_surfaceType_lr == 1 || current.gp2_surfaceType_lr == 2 ||
        current.gp2_surfaceType_rr == 1 || current.gp2_surfaceType_rr == 2);

    static bool wasOnKerb = false;
    static bool effectStarted = false;

    if (onKerb && current.gp2_speedKmh > 5.0) {
        if (!wasOnKerb) {
            LogMessage(L"[VIBRATION DEBUG] KERB DETECTED! Speed: " + std::to_wstring(current.gp2_speedKmh));
        }

        // Speed scaling - stronger at all speeds but scales with speed
        double speedFactor;
        if (current.gp2_speedKmh < 50.0) {
            speedFactor = 0.5 + (current.gp2_speedKmh - 5.0) / 45.0 * 0.3;  // 0.5 to 0.8 range
        }
        else if (current.gp2_speedKmh < 200.0) {
            speedFactor = 0.8 + ((current.gp2_speedKmh - 50.0) / 150.0) * 0.2;  // 0.8 to 1.0 range
        }
        else {
            speedFactor = 1.0;  // Full strength above 200kph
        }

        // Count tires on kerb for intensity scaling
        int tiresOnKerb = 0;
        if (current.gp2_surfaceType_lf == 1 || current.gp2_surfaceType_lf == 2) tiresOnKerb++;
        if (current.gp2_surfaceType_rf == 1 || current.gp2_surfaceType_rf == 2) tiresOnKerb++;
        if (current.gp2_surfaceType_lr == 1 || current.gp2_surfaceType_lr == 2) tiresOnKerb++;
        if (current.gp2_surfaceType_rr == 1 || current.gp2_surfaceType_rr == 2) tiresOnKerb++;

        double tireIntensity = 0.7 + (tiresOnKerb * 0.075);
        if (tireIntensity > 1.0) tireIntensity = 1.0;

        // Calculate final intensity (vibrationForceScale is 0.0-1.0)
        double finalIntensity = speedFactor * tireIntensity * vibrationForceScale;

        // Calculate magnitude - strong base force for noticeable kerb effect
        int calculatedMagnitude = static_cast<int>(finalIntensity * 4000.0);

        // Apply minimum threshold if needed
        int finalMagnitude = calculatedMagnitude;
        if (vibrationForceScale > 0.0 && calculatedMagnitude < 500) {
            finalMagnitude = static_cast<int>(500 * vibrationForceScale);
        }

        // Only log detailed calculations every 5 seconds while on kerb
        if (debugCounter % 300 == 0) {
            LogMessage(L"[VIBRATION DEBUG] SpeedFactor: " + std::to_wstring(speedFactor) +
                L", TireIntensity: " + std::to_wstring(tireIntensity) +
                L", FinalIntensity: " + std::to_wstring(finalIntensity) +
                L", CalcMag: " + std::to_wstring(calculatedMagnitude) +
                L", FinalMag: " + std::to_wstring(finalMagnitude));
        }

        // Set up the periodic effect parameters
        DIPERIODIC periodicForce = {};
        periodicForce.dwMagnitude = finalMagnitude;
        periodicForce.lOffset = 0;
        periodicForce.dwPhase = 0;
        periodicForce.dwPeriod = DI_SECONDS / 20;  // 20Hz for strong, noticeable vibration

        DIEFFECT eff = {};
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.dwDuration = INFINITE;
        eff.dwGain = 10000;  // Maximum gain
        eff.dwTriggerButton = DIEB_NOTRIGGER;
        eff.cAxes = 1;
        DWORD axes[1] = { DIJOFS_X };
        LONG dir[1] = { 0 };
        eff.rgdwAxes = axes;
        eff.rglDirection = dir;
        eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
        eff.lpvTypeSpecificParams = &periodicForce;

        // Update effect parameters
        HRESULT hr = periodicVibrationEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_DURATION | DIEP_GAIN);

        if (FAILED(hr)) {
            LogMessage(L"[VIBRATION ERROR] SetParameters failed: 0x" + std::to_wstring(hr));
        }

        // Start effect if not already started
        if (!effectStarted) {
            hr = periodicVibrationEffect->Start(1, 0);
            if (SUCCEEDED(hr)) {
                effectStarted = true;
                LogMessage(L"[VIBRATION] Started periodic effect, magnitude: " + std::to_wstring(finalMagnitude));
            }
            else {
                LogMessage(L"[VIBRATION ERROR] Start failed: 0x" + std::to_wstring(hr));
            }
        }

        wasOnKerb = true;

    }
    else {
        // Stop vibration when off kerb
        if (wasOnKerb && effectStarted) {
            HRESULT hr = periodicVibrationEffect->Stop();
            if (SUCCEEDED(hr)) {
                LogMessage(L"[VIBRATION] Stopped periodic effect");
            }
            else {
                LogMessage(L"[VIBRATION ERROR] Stop failed: 0x" + std::to_wstring(hr));
            }
            effectStarted = false;
            wasOnKerb = false;
        }
    }
}

HRESULT CreatePeriodicVibrationEffect(IDirectInputDevice8* device, IDirectInputEffect** periodicVibrationEffect) {
    if (!device || !periodicVibrationEffect) return E_INVALIDARG;

    DIPERIODIC periodicForce = {};
    periodicForce.dwMagnitude = 0;  // Start with zero
    periodicForce.lOffset = 0;
    periodicForce.dwPhase = 0;
    periodicForce.dwPeriod = DI_SECONDS / 30;  // 30Hz default

    DIEFFECT eff = {};
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwGain = 10000;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.cAxes = 1;
    DWORD axes[1] = { DIJOFS_X };
    LONG dir[1] = { 0 };
    eff.rgdwAxes = axes;
    eff.rglDirection = dir;
    eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
    eff.lpvTypeSpecificParams = &periodicForce;

    HRESULT hr = device->CreateEffect(GUID_Sine, &eff, periodicVibrationEffect, nullptr);
    if (FAILED(hr)) {
        LogMessage(L"[ERROR] Failed to create periodic vibration effect. HRESULT: 0x" + std::to_wstring(hr));
        *periodicVibrationEffect = nullptr;
    }
    else {
        LogMessage(L"[INFO] Periodic vibration effect created successfully");
    }

    return hr;
}

void CleanupPeriodicEffect(IDirectInputEffect* periodicVibrationEffect) {
    if (periodicVibrationEffect) {
        periodicVibrationEffect->Stop();
        periodicVibrationEffect->Release();
        LogMessage(L"[INFO] Periodic vibration effect cleaned up");
    }
}