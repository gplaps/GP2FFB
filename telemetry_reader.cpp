// telemetry_reader.cpp
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "telemetry_reader.h"
#include "ffb_setup.h"
#include "cmath"

#define FRONT_LEFT 1 // 0
#define FRONT_RIGHT 3 // 1
#define REAR_LEFT 0 // 2
#define REAR_RIGHT 2 // 3

#define SURFACE_ASPHALT 0
#define SURFACE_LOW_CURB 1
#define SURFACE_HIGH_CURB 2
#define SURFACE_GRASS 3
#define SURFACE_GRAVEL 4

#define WD_FRONT_LEFT (2 * 512) // 2 
#define WD_FRONT_RIGHT (3 * 512) // 3
#define WD_REAR_LEFT (0 * 512) // 0
#define WD_REAR_RIGHT (1 * 512) // 1 


#define INT_SIZE 32
#define SHORT_SIZE 16
#define BYTE_SIZE 8

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



typedef struct {
    int structSize;

    int  deviceID; // just the enumerated ID, not the GUID
    wchar_t deviceName[260];

    bool isInRace; // to not apply anything when not in a race, there could be bogus data left in memory
    bool isPaused;
    bool isReplay;
    bool isX86GP2MenuOn;
    bool isPlayer; // to not have any FBB when the POV is not the player

    float fps;

    // these are in real world units
    float speedKmh;

    float stWheelAngle;
    float tyreTurnAngle;

    float slipAngleFront;
    float slipAngleRear;

    int surfaceType[4];

    // i don't know the scaling or the unit for these
    int suspensionTravel[4]; // could this indirectly tell the tyre load?
    int rideHeights[4]; // this could be more in world space than in relation to the car because it goes very high when you get lifted in the box
    int wheelSpin_13C[4]; // this is the rotation of the wheels
    int notOnDamper[4]; // someone else named it like this, i don't know what he meant by not on damper
    int calc_248[4]; // no clue
    int wheel_2AC[4]; // no clue

    unsigned char wheelsData[2048];
} SharedMemory;

static HANDLE hmap = NULL;
static const SharedMemory* p = nullptr;
static bool initialized = false;

int readWheelData(const unsigned char* wheelsData, int dataStartOffset, int offsetIntoData) {
    return *reinterpret_cast<const int*>(&wheelsData[dataStartOffset + offsetIntoData]);
}

// === Main ===

bool ReadTelemetryData(RawTelemetry& out) {

    if (!initialized) {
        hmap = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\x86GP2FFB");
        if (!hmap) {
            LogMessage(L"x86GP2 is not found");
            return false;
        }

        p = (const SharedMemory*)MapViewOfFile(hmap, FILE_MAP_READ, 0, 0, 0);
        if (!p) {
            CloseHandle(hmap);
            return false;
        }
        initialized = true;
    }

    CONSOLE_CURSOR_INFO ci = { 1, FALSE };
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);

    out.gp2_structSize = p->structSize;
    
    out.gp2_isInRace = p->isInRace;
    out.gp2_isPlayer = p->isPlayer;
    out.gp2_isPaused = p->isPaused;
    out.gp2_isReplay = p->isReplay;
    out.gp2_isX86MenuOn = p->isX86GP2MenuOn;
    out.gp2_deviceID = p->deviceID;


    out.gp2_speedKmh = p->speedKmh;
    out.gp2_stWheelAngle = p->stWheelAngle;
    out.gp2_tyreTurnAngle = p->tyreTurnAngle;
    out.gp2_slipAngleFront = p->slipAngleFront;
    out.gp2_slipAngleRear = p->slipAngleRear;

    out.gp2_magLat_lf = static_cast<float>(readWheelData(p->wheelsData, WD_FRONT_LEFT, 52));
    out.gp2_magLat_rf = static_cast<float>(readWheelData(p->wheelsData, WD_FRONT_RIGHT, 52));

    out.gp2_magLong_lf = static_cast<float>(readWheelData(p->wheelsData, WD_FRONT_LEFT, 380));
    out.gp2_magLong_rf = static_cast<float>(readWheelData(p->wheelsData, WD_FRONT_RIGHT, 380));

    out.gp2_surfaceType_lf = p->surfaceType[FRONT_LEFT];
    out.gp2_surfaceType_rf = p->surfaceType[FRONT_RIGHT];
    out.gp2_surfaceType_lr = p->surfaceType[REAR_LEFT];
    out.gp2_surfaceType_rr = p->surfaceType[REAR_RIGHT];

    out.gp2_rideHeights_lf = p->rideHeights[FRONT_LEFT];
    out.gp2_rideHeights_rf = p->rideHeights[FRONT_RIGHT];
    out.gp2_rideHeights_lr = p->rideHeights[REAR_LEFT];
    out.gp2_rideHeights_rr = p->rideHeights[REAR_RIGHT];

    out.gp2_wheelSpin_13C_lf = p->wheelSpin_13C[FRONT_LEFT];
    out.gp2_wheelSpin_13C_rf = p->wheelSpin_13C[FRONT_RIGHT];
    out.gp2_wheelSpin_13C_lr = p->wheelSpin_13C[REAR_LEFT];
    out.gp2_wheelSpin_13C_rr = p->wheelSpin_13C[REAR_RIGHT];

    out.gp2_notOnDamper_lf = p->notOnDamper[FRONT_LEFT];
    out.gp2_notOnDamper_rf = p->notOnDamper[FRONT_RIGHT];
    out.gp2_notOnDamper_lr = p->notOnDamper[REAR_LEFT];
    out.gp2_notOnDamper_rr = p->notOnDamper[REAR_RIGHT];

    out.gp2_calc_248_lf = p->calc_248[FRONT_LEFT];
    out.gp2_calc_248_rf = p->calc_248[FRONT_RIGHT];
    out.gp2_calc_248_lr = p->calc_248[REAR_LEFT];
    out.gp2_calc_248_rr = p->calc_248[REAR_RIGHT];

    out.gp2_wheel_2AC_lf = p->wheel_2AC[FRONT_LEFT];
    out.gp2_wheel_2AC_rf = p->wheel_2AC[FRONT_RIGHT];
    out.gp2_wheel_2AC_lr = p->wheel_2AC[REAR_LEFT];
    out.gp2_wheel_2AC_rr = p->wheel_2AC[REAR_RIGHT];

    out.valid = true;

    return true;
}