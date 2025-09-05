﻿#include "constant_force.h"
#include <iostream>
#include <algorithm>
#include <deque>
#include <numeric>

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


// Get config data
extern std::wstring targetInvertFFB;

// To be used in reporting
extern int g_currentFFBForce;
extern int g_currentFrontLoad;


void ApplyConstantForceEffect(const RawTelemetry& current,
    const CalculatedVehicleDynamics& vehicleDynamics,
    double gp2_speedKmh,
    IDirectInputEffect* constantForceEffect,
    bool enableVibrationForce,
    bool enableWeightForce,
    bool enableRateLimit,
    double masterForceScale,
    double deadzoneForceScale,
    double constantForceScale,
    double vibrationForceScale,
    double brakingForceScale,
    double weightForceScale
) {

    if (!constantForceEffect) return;

    // Beta 0.5
    // This is a bunch of logic to pause/unpause or prevent forces when the game isn't running

    static double lastKph = 0.0;
    static bool hasEverMoved = false;
    static int noMovementFrames = 0;
    static bool isPaused = true;
    static bool pauseForceSet = false;
    static bool isFirstReading = true;
    static bool lastInactiveState = true; // Track previous state
    constexpr int movementThreshold = 10;
    constexpr double movementThreshold_value = 0.001;

    if (isFirstReading) {
        lastKph = current.gp2_speedKmh;
        isFirstReading = false;
        if (!pauseForceSet) {
            DICONSTANTFORCE cf = { 0 };
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
            eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
            eff.lpvTypeSpecificParams = &cf;
            constantForceEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION);
            pauseForceSet = true;
        }
        return;
    }

    bool isInactive = (!current.gp2_isInRace || current.gp2_isPaused || current.gp2_isReplay || current.gp2_isX86MenuOn);

    if (isInactive) {
        if (!lastInactiveState) { // Only log when transitioning to inactive
            LogMessage(L"[INFO] Game paused detected - sending zero force");
        }
        isPaused = true;
        pauseForceSet = false;
        lastInactiveState = true;
    }
    else {
        if (lastInactiveState) { // Only log when transitioning to active
            LogMessage(L"[INFO] Game resumed - restoring normal forces");
        }
        isPaused = false;
        pauseForceSet = false;
        lastInactiveState = false;
        lastKph = current.gp2_speedKmh;
    }


    // If paused, send zero force and return
    if (isPaused) {
        if (!pauseForceSet) {
            DICONSTANTFORCE cf = { 0 };
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
            eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
            eff.lpvTypeSpecificParams = &cf;
            constantForceEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION);
            pauseForceSet = true;
        }
        return;
    }


    // Low speed filtering
    /*
    if (gp2_speedKmh < 5.0) {
        static bool wasLowSpeed = false;
        if (!wasLowSpeed) {
            // Send zero force when entering low speed
            DICONSTANTFORCE cf = { 0 };
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
            eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
            eff.lpvTypeSpecificParams = &cf;
            constantForceEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION);
            wasLowSpeed = true;
        }
     
        return;
    }
       */
    static bool wasLowSpeed = false;
    if (wasLowSpeed) {
        wasLowSpeed = false;  // Reset when speed picks up
    }


// === CALC 1 - Lateral G ===

// This will add a 'base' force based on speed. This helps INCREDIBLY with straight line control
// It gets rid of that 'ping pong' effect that I've been chasing while not making the forces feel delayed
/*
    double baseLoad = 0.0;
    if (speed_mph > 60.0) {
        double speedFactor = (speed_mph - 60.0) / 180.0;  // 0.0 at 120mph, 1.0 at 220mph
        if (speedFactor > 1.0) speedFactor = 1.0;
        baseLoad = speedFactor * speedFactor * 1200.0;    // Amount of force to apply at 'peak'
    }

    // === G-Force cornering ===
    double absG = std::abs(vehicleDynamics.lateralG);
    double corneringForce = 0.0;

    double configuredDeadzone = 0.03 * deadzoneForceScale;  // Version 0.8 Beta added Deadzone to ini

    if (absG > configuredDeadzone) {
        double effectiveG = absG - configuredDeadzone;

        // Adjust the calculation range based on deadzone
        double maxEffectiveG = 4.0 - configuredDeadzone;

        if (effectiveG <= maxEffectiveG) {
            double normalizedG = effectiveG / maxEffectiveG;
            double curveValue = normalizedG * (0.7 + 0.3 * normalizedG);
            corneringForce = curveValue * 8000.0;
        }
        else {
            corneringForce = 8000.0;
        }
    }

    // === Final force calculation ===
    double force;

    if (vehicleDynamics.lateralG > 0.03) {
        // Right turn
        force = baseLoad + corneringForce;
    }
    else if (vehicleDynamics.lateralG < -0.03) {
        // Left turn
        force = baseLoad + corneringForce;
    }
    else {
        // Straight line - very light
        force = baseLoad * 0.2;  // Even lighter when straight
    }
*/

// Niels Equation
// Calculate based on front tire load directly instead of linear G = Much better
// No issues with oscillations anymore

// Get the sum with signs preserved
    //double frontTireLoadSum = vehicleDynamics.frontLeftForce_N + vehicleDynamics.frontRightForce_N; //original lat forces only

    double longScaler = 15 * (brakingForceScale/100);

    double frontTireLongSum = (vehicleDynamics.frontRightLong_N - vehicleDynamics.frontLeftLong_N) * longScaler;
   
   // double frontTireLoadSum = (vehicleDynamics.frontLeftForce_N + vehicleDynamics.frontRightForce_N) + frontTireLongSum;

   // double frontTireLoadSum = (std::abs(vehicleDynamics.frontLeftForce_N) - std::abs(vehicleDynamics.frontRightForce_N) + frontTireLongSum);
    
    double leftForce = vehicleDynamics.frontLeftForce_N;
    double rightForce = vehicleDynamics.frontRightForce_N;

    // Reduce force for the wheel with less grip when off asphalt
    if (current.gp2_surfaceType_rf != 0) {
        if (rightForce >= 0) {
            rightForce = leftForce * 0.15;  // Reduce the force on the wheel with less grip        
        }
    }

    if (current.gp2_surfaceType_lf != 0) {
        if (leftForce <= 0) {
            leftForce = rightForce * 0.15;  // Reduce the force on the wheel with less grip

        }
    }

     double frontTireLoadSum = (std::abs(leftForce) - std::abs(rightForce)) + frontTireLongSum;

   // double frontTireLoadSum = leftForce + rightForce + frontTireLongSum;


    // double frontTireLoadSum = frontTireLongSum;

    //double frontTireLoadSum = std::abs(vehicleDynamics.frontLeftForce_N) - std::abs(vehicleDynamics.frontRightForce_N);


    //double frontTireLoadSum = frontTireLongSum;

    // === Input Smoothing ===
/*    static double smoothedFrontTireLoadSum = 0.0;
    static bool inputInitialized = false;
    const double INPUT_SMOOTHING = 0.4;  // Adjust 0.1-0.4 based on preference

    if (!inputInitialized) {
        smoothedFrontTireLoadSum = frontTireLoadSum;
        inputInitialized = true;
    }
    else {
        smoothedFrontTireLoadSum = (INPUT_SMOOTHING * frontTireLoadSum) +
            ((1.0 - INPUT_SMOOTHING) * smoothedFrontTireLoadSum);
    }
*/

// === IMPROVED SMOOTHING WITH MULTIPLE STRATEGIES ===

     static double smoothedFrontTireLoadSum = 0.0;
     static bool inputInitialized = false;
     const double INPUT_SMOOTHING = 0.4;  // Adjust 0.1-0.4 based on preference

     if (!inputInitialized) {
         smoothedFrontTireLoadSum = frontTireLoadSum;
         inputInitialized = true;
     }
     else {
         smoothedFrontTireLoadSum = (INPUT_SMOOTHING * frontTireLoadSum) +
             ((1.0 - INPUT_SMOOTHING) * smoothedFrontTireLoadSum);
     }


    // Use smoothed input for all calculations
   // double frontTireLoadMagnitude = std::abs(smoothedFrontTireLoadSum);

     double frontTireLoadMagnitude = std::abs(smoothedFrontTireLoadSum);


   // double frontTireLoadSum = (vehicleDynamics.frontLeftForce_N + (vehicleDynamics.frontLeftLong_N * longScaler)) + (vehicleDynamics.frontRightForce_N + (vehicleDynamics.frontRightLong_N * longScaler));

    // Use the magnitude for physics calculation
     // Use the magnitude for physics calculation
    //double frontTireLoadMagnitude = std::abs(frontTireLoadSum);

    // Keep frontTireLoad for logging compatibility
    double frontTireLoad = frontTireLoadMagnitude;

    g_currentFrontLoad = frontTireLoad;


/*
        // Physics constants from your engineer friend
        const double CURVE_STEEPNESS = 4.0017421865e-4;
        const double MAX_THEORETICAL = 5464.666;
        const double SCALE_FACTOR = 1.2020;

        // Calculate magnitude using physics formula
        double physicsForceMagnitude = atan(frontTireLoadMagnitude * CURVE_STEEPNESS) * MAX_THEORETICAL * SCALE_FACTOR;

        // Apply the natural sign from the tire load sum
        double physicsForce = (frontTireLoadSum >= 0) ? physicsForceMagnitude : -physicsForceMagnitude;

        // Apply proportional deadzone
        double force = physicsForce;
        if (deadzoneForceScale > 0.0) {
            // Convert deadzoneForceScale (0-100) to percentage (0.0-1.0)
            double deadzonePercentage = deadzoneForceScale / 100.0;

            // Calculate deadzone threshold using magnitude
            double maxPossibleForce = MAX_THEORETICAL * SCALE_FACTOR; // ~10200
            double deadzoneThreshold = maxPossibleForce * deadzonePercentage;

            // Apply deadzone: remove bottom X% and rescale remaining range
            if (std::abs(physicsForce) <= deadzoneThreshold) {
                force = 0.0;  // Force is in deadzone - zero output
            }
            else {
                // Rescale remaining force range to maintain full output range
                double remainingRange = maxPossibleForce - deadzoneThreshold;
                double adjustedInput = std::abs(physicsForce) - deadzoneThreshold;
                double scaledMagnitude = (adjustedInput / remainingRange) * maxPossibleForce;

                // Restore original sign
                force = (physicsForce >= 0) ? scaledMagnitude : -scaledMagnitude;
            }
          }
*/


    double physicsForceMagnitude;
    
    const double STEEP_THRESHOLD = 1500.0;     // Switch point: 1500N
    const double STEEP_FORCE_TARGET = 2500.0;  // Force at switch point: 2000
    const double GENTLE_LOAD_TARGET = 20000.0; // High load point: 14000N
    const double GENTLE_FORCE_TARGET = 9500.0; // Force at high load: 8500

    if (frontTireLoadMagnitude <= STEEP_THRESHOLD) {
        // Steep linear ramp: 0N→0 force, 1500N→2000 force
        double steepSlope = STEEP_FORCE_TARGET / STEEP_THRESHOLD; // 2000/1500 = 1.333
        physicsForceMagnitude = frontTireLoadMagnitude * steepSlope;
    }
    else {
        // Gentler slope: 1500N→2000 force, 14000N→8500 force
        double remainingLoad = frontTireLoadMagnitude - STEEP_THRESHOLD;        // Load above 1500N
        double remainingLoadRange = GENTLE_LOAD_TARGET - STEEP_THRESHOLD;       // 14000-1500 = 12500N
        double remainingForceRange = GENTLE_FORCE_TARGET - STEEP_FORCE_TARGET;  // 8500-2000 = 6500 force

        double gentleSlope = remainingForceRange / remainingLoadRange;          // 6500/12500 = 0.52
        double gentleSlopeForce = remainingLoad * gentleSlope;

        physicsForceMagnitude = STEEP_FORCE_TARGET + gentleSlopeForce;
    }

    // Cap at maximum to prevent going over target
    if (physicsForceMagnitude > GENTLE_FORCE_TARGET) {
        physicsForceMagnitude = GENTLE_FORCE_TARGET;
    }

    // Apply the original sign from frontTireLoadSum
    double physicsForce = (frontTireLoadSum >= 0) ? physicsForceMagnitude : -physicsForceMagnitude;


    double force = physicsForce;

    //== LOG Curve, too 'notchy'
    /*
    // Curve Parameters
    // Added step for greater center feel
    const double LOG_SCALE = 0.01; // Curve steepness
    const double STEEP_THRESHOLD = 1800.0; // Transition Point
    const double STEEP_FORCE_TARGET = 2500.0; // FFB % at transition
    const double GENTLE_LOAD_TARGET = 18000.0; // Max Load
    const double GENTLE_FORCE_TARGET = 9800.0; // FFB % at max load
    const double GENTLE_SLOPE = (GENTLE_FORCE_TARGET - STEEP_FORCE_TARGET) / (GENTLE_LOAD_TARGET - STEEP_THRESHOLD);


    if (frontTireLoadMagnitude <= STEEP_THRESHOLD) {
        // Logarithmic portion (smooth curve from 0 to transition point)
        // Excel formula: $E$3 * (LN(1 + A2 * $E$1) / LN(1 + $E$2 * $E$1))

        //log
        double numerator = log(1.0 + frontTireLoadMagnitude * LOG_SCALE);
        double denominator = log(1.0 + STEEP_THRESHOLD * LOG_SCALE);

        //exp

        //double numerator = (exp(LOG_SCALE * frontTireLoadMagnitude / STEEP_THRESHOLD)) - 1;
        //double denominator = exp(STEEP_THRESHOLD) - 1;

        physicsForceMagnitude = STEEP_FORCE_TARGET * (numerator / denominator);
    }
    else {
        // Linear portion after transition point
        // Excel formula: ((A2-$E$2)*$H$5)+$E$3

        double remainingLoad = frontTireLoadMagnitude - STEEP_THRESHOLD;
        physicsForceMagnitude = (remainingLoad * GENTLE_SLOPE) + STEEP_FORCE_TARGET;

    }

    // Apply the original sign from frontTireLoadSum
    double physicsForce = (frontTireLoadSum >= 0) ? physicsForceMagnitude : -physicsForceMagnitude;

    double force = physicsForce;
*/

    // Apply proportional deadzone
    if (deadzoneForceScale > 0.0) {
        // Convert deadzoneForceScale (0-100) to percentage (0.0-1.0)
        double deadzonePercentage = deadzoneForceScale / 100.0;

        // Calculate deadzone threshold using magnitude
        double maxPossibleForce = 10000; // ~10200
        double deadzoneThreshold = maxPossibleForce * deadzonePercentage;

        // Apply deadzone: remove bottom X% and rescale remaining range
        if (std::abs(physicsForce) <= deadzoneThreshold) {
            force = 0.0;  // Force is in deadzone - zero output
        }
        else {
            // Rescale remaining force range to maintain full output range
            double remainingRange = maxPossibleForce - deadzoneThreshold;
            double adjustedInput = std::abs(physicsForce) - deadzoneThreshold;
            double scaledMagnitude = (adjustedInput / remainingRange) * maxPossibleForce;

            // Restore original sign
            force = (physicsForce >= 0) ? scaledMagnitude : -scaledMagnitude;
        }
    }


/*
    // ==== CENTERING FORCE =====
    //Fake centering force to add feeling to the center of the wheel since the data doesnt give it
    double centeringForce = 0.0;
    if (gp2_speedKmh > 5.0) {
        double baseStrengthPerDegree = 2000.0 / 3.0;  // ~667 per deg
        double speedFactor = std::clamp(gp2_speedKmh / 200.0, 0.3, 1.0);

        centeringForce = -current.gp2_stWheelAngle * baseStrengthPerDegree * speedFactor;
    }

    // Blend: full centering at 0°, taper to 0 by ±3°, pure physics beyond
    double angleNorm = std::abs(current.gp2_stWheelAngle) / 5.0;
    angleNorm = std::clamp(angleNorm, 0.0, 1.0);

    // Quadratic easing so it fades smoothly
    double blend = 1.0 - (angleNorm * angleNorm);

    // Apply blend (centering only contributes when blend > 0)
    force = blend * centeringForce + force;
*/

       
 

    // Cap maximum force magnitude while preserving sign
    if (std::abs(force) > 10000.0) {
        force = (force >= 0) ? 10000.0 : -10000.0;
    }

    // Handle invert option (no more complex direction logic needed!)
    bool invert = (targetInvertFFB == L"true" || targetInvertFFB == L"True");
    if (invert) {
        force = -force;
    }

    // Convert to signed magnitude

    double smoothed = force;
    int magnitude = static_cast<int>(std::abs(smoothed) * masterForceScale * constantForceScale);
    //int signedMagnitude = static_cast<int>(magnitude);
    //if (smoothed < 0.0) {
    //    signedMagnitude = -signedMagnitude;
   // }


    double scaledForce = force * masterForceScale * constantForceScale;
    int signedMagnitude = static_cast<int>(scaledForce);



 
    // === Output Smoothing ===

    // Take final magnitude and prevent any massive jumps over a small frame range

    static std::deque<int> magnitudeHistory;
    magnitudeHistory.push_back(signedMagnitude);
    if (magnitudeHistory.size() > 2) {
        magnitudeHistory.pop_front();
    }
    signedMagnitude = static_cast<int>(std::accumulate(magnitudeHistory.begin(), magnitudeHistory.end(), 0.0) / magnitudeHistory.size());


    // === CALC 3 Weight Force ===
        // Weight shifting code
        // This will try to create some feeling based on the front tire loads changing to hopefully 'feel' the road more
        // It watches for changes in the left to right split of force
        // 
        // If the front left tire is doing 45% of the work and right front is doing 55%
        // And then they change and the left is doing 40% and the right is 60%
        // You will feel a bump
        // This adds detail to camber and surface changes
    /*
    if (enableWeightForce && speed_mph > 1.0) {
        static double lastFrontImbalance = 0.0;
        static double weightTransferForce = 0.0;
        static int framesSinceChange = 0;

        double totalFrontLoad = vehicleDynamics.frontLeftForce_N + vehicleDynamics.frontRightForce_N;
        if (totalFrontLoad > 100.0) {
            double currentImbalance = (vehicleDynamics.frontLeftForce_N - vehicleDynamics.frontRightForce_N) / totalFrontLoad;
            double imbalanceChange = currentImbalance - lastFrontImbalance;


            if (std::abs(imbalanceChange) > 0.005) {  // How much of a change to consider between left/right split
                // Much stronger force
                weightTransferForce += imbalanceChange * 4500.0;  // Force scaler
                framesSinceChange = 0;

                // Higher cap for stronger effects
                weightTransferForce = std::clamp(weightTransferForce, -5000.0, 5000.0);
            }
            else {
                framesSinceChange++;
            }

            // Decay time to hold effect, don't want to hold it forever because then we dont feel regular force
            if (framesSinceChange > 60) {
                weightTransferForce *= 0.995;  // decay time per frame, inverse so 0.005% per frame

                if (std::abs(weightTransferForce) < 20.0) {
                    weightTransferForce = 0.0;
                }
            }

            signedMagnitude += static_cast<int>((weightTransferForce * masterForceScale) * weightForceScale);
            lastFrontImbalance = currentImbalance;
        }
    }
    */

    /*
    // === Reduce update Rate ===
    //
        // This will reduce the rate at which we send updates to the wheel
        // The idea is to not break older controllers or ones which can't take frequent updates
        // The risk is adding delay


        static int lastSentMagnitude = -1;
        static int lastSentSignedMagnitude = 0;
        static int lastProcessedMagnitude = -1;
        static int framesSinceLastUpdate = 0;
        static double accumulatedMagnitudeChange = 0.0;
        static double accumulatedSignChange = 0.0;


        if (lastSentMagnitude != -1) {
            accumulatedMagnitudeChange += std::abs(magnitude - lastProcessedMagnitude);
            accumulatedSignChange += std::abs(signedMagnitude - lastSentSignedMagnitude);
        }

        framesSinceLastUpdate++;
        bool shouldUpdate = false;

        // 1. More sensitive immediate changes
        if (std::abs(magnitude - lastSentMagnitude) >= 400 ||     // Reduced from 400
            std::abs(signedMagnitude - lastSentSignedMagnitude) >= 2000) { // Reduced from 800
            shouldUpdate = true;
        }
        // 2. More sensitive accumulated changes
        else if (accumulatedMagnitudeChange >= 300 ||             // Reduced from 300
            accumulatedSignChange >= 1500) {                   // Reduced from 600
            shouldUpdate = true;
        }
        // 3. Direction change (unchanged - always important)
        else if ((lastSentSignedMagnitude > 0 && signedMagnitude < 0) ||
            (lastSentSignedMagnitude < 0 && signedMagnitude > 0) ||
            (lastSentSignedMagnitude == 0 && signedMagnitude != 0) ||
            (lastSentSignedMagnitude != 0 && signedMagnitude == 0)) {
            shouldUpdate = true;
        }
        // 4. Timeout, how long until we send an update no matter what
        else if (framesSinceLastUpdate >= 12) {                    // Reduced from 12 (~15Hz)
            shouldUpdate = true;
        }
        // 5. Zero force (unchanged)
        else if (magnitude == 0 && lastSentMagnitude != 0) {
            shouldUpdate = true;
        }

        if (!shouldUpdate) {
            lastProcessedMagnitude = magnitude;
            return;
        }

        // Reset tracking
        lastSentMagnitude = magnitude;
        lastSentSignedMagnitude = signedMagnitude;
        framesSinceLastUpdate = 0;
        accumulatedMagnitudeChange = 0.0;
        accumulatedSignChange = 0.0;
        lastProcessedMagnitude = magnitude;

        */


        // === Rate Limiting ===
    // === Reimplemnted older style to try to make compatible with Thrustmaster wheels ===
    if (enableRateLimit) {
        // Direction calculation and smoothing for rate limiting
        LONG targetDir = (smoothed > 0.0 ? -1 : (smoothed < 0.0 ? 1 : 0)) * 10000;
        static LONG lastDirection = 0;

        // Direction smoothing - this prevents rapid direction changes
        constexpr double directionSmoothingFactor = 0.3;
        lastDirection = static_cast<LONG>((1.0 - directionSmoothingFactor) * lastDirection + directionSmoothingFactor * targetDir);

        // Rate limiting with direction smoothing
        static int lastSentMagnitude = -1;
        static int lastSentSignedMagnitude = 0;
        static LONG lastSentDirection = 0;  // Track smoothed direction
        static int lastProcessedMagnitude = -1;
        static int framesSinceLastUpdate = 0;
        static double accumulatedMagnitudeChange = 0.0;
        static double accumulatedDirectionChange = 0.0;  // Track direction changes

        // Track accumulated changes since last update
        if (lastSentMagnitude != -1) {
            accumulatedMagnitudeChange += std::abs(magnitude - lastProcessedMagnitude);
            accumulatedDirectionChange += std::abs(lastDirection - lastSentDirection);  // Use smoothed direction
        }

        framesSinceLastUpdate++;
        bool shouldUpdate = false;

        // 1. Large immediate change
        if (std::abs(magnitude - lastSentMagnitude) >= 400 ||
            std::abs(lastDirection - lastSentDirection) >= 2000) {  // Use smoothed direction
            shouldUpdate = true;
        }
        // 2. Accumulated changes
        else if (accumulatedMagnitudeChange >= 300 ||
            accumulatedDirectionChange >= 1500) {  // Use direction accumulation
            shouldUpdate = true;
        }
        // 3. Direction sign change (use smoothed direction)
        else if ((lastSentDirection > 0 && lastDirection < 0) ||
            (lastSentDirection < 0 && lastDirection > 0) ||
            (lastSentDirection == 0 && lastDirection != 0) ||
            (lastSentDirection != 0 && lastDirection == 0)) {
            shouldUpdate = true;
        }
        // 4. Timeout
        else if (framesSinceLastUpdate >= 12) {
            shouldUpdate = true;
        }
        // 5. Zero force
        else if (magnitude == 0 && lastSentMagnitude != 0) {
            shouldUpdate = true;
        }

        if (!shouldUpdate) {
            lastProcessedMagnitude = magnitude;
            return;  // Skip this frame
        }

        // Reset tracking when we send an update
        lastSentMagnitude = magnitude;
        lastSentSignedMagnitude = signedMagnitude;
        lastSentDirection = lastDirection;  // Track the smoothed direction
        framesSinceLastUpdate = 0;
        accumulatedMagnitudeChange = 0.0;
        accumulatedDirectionChange = 0.0;
        lastProcessedMagnitude = magnitude;
    }

    g_currentFFBForce = signedMagnitude;

    //Logging
    static int debugCounter = 0;
    if (debugCounter % 1 == 0) {  // Every 30 frames
        LogMessage(L"[DEBUG] FL: " + std::to_wstring(vehicleDynamics.frontLeftForce_N) +
            L", FR: " + std::to_wstring(vehicleDynamics.frontRightForce_N) +
            L", Total: " + std::to_wstring(frontTireLoad) +
            L", atan_input: " + std::to_wstring(frontTireLoad * 1.0e-4) +
            L", atan_result: " + std::to_wstring(atan(frontTireLoad * 1.0e-4)));
    }
    debugCounter++;


    DICONSTANTFORCE cf = { signedMagnitude };  // Use signed magnitude
    DIEFFECT eff = {};
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwGain = 10000;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.cAxes = 1;
    DWORD axes[1] = { DIJOFS_X };
    LONG dir[1] = { 0 };  // ← Always zero direction now, this broke Moza wheels
    eff.rgdwAxes = axes;
    eff.rglDirection = dir;
    eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams = &cf;

    // Only set magnitude params, skip direction
    HRESULT hr = constantForceEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS);  // ← Removed | DIEP_DIRECTION

    if (FAILED(hr)) {
        std::wcerr << L"Constant force SetParameters failed: 0x" << std::hex << hr << std::endl;
    }
}