// FFB for Grand Prix II x86 version
// I don't know what I am doing!


// File: main.cpp

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
 

// === Standard Library Includes ===
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <deque>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <string>
#include <sstream>

// === Windows & DirectInput ===
#include <windows.h>
#include <dinput.h>

// === Project Includes ===
#include "ffb_setup.h"
#include "telemetry_reader.h"
#include "calculations/vehicle_dynamics.h"
#include "forces/constant_force.h"
#include "forces/damper_effect.h"
#include "forces/spring_effect.h"

// Global timing buffers
LARGE_INTEGER start, end, frequency;
double printTime = 0.0, telemetryTime = 0.0, FFBTime = 0.0;

#define PRINT_INTERVAL 66.68     // log timing ~15fps
#define TELEMETRY_INTERVAL 16.67  // ~60 FPS telemetry
#define FFB_INTERVAL 16.67        // ~60 FPS FFB

double getPerformanceCounterTime() {
    QueryPerformanceCounter(&end);
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
}

// Global log buffer
std::mutex logMutex;
std::deque<std::wstring> logLines;
const size_t maxLogLines = 1000;  // Show last 1000 log lines

// === Global Force Feedback Flags & States ===
// I have 3 effects right now which all get calculated separately
// I think probably all you need are these three to make good FFB

// Constant force is the most in depth
// Damper & Spring just use speed to do things
bool enableRateLimit = false;
bool enableConstantForce = false;
bool enableWeightForce = false;
bool enableDamperEffect = false;
bool enableSpringEffect = false;

bool constantStarted = false;
bool damperStarted = false;
bool springStarted = false;

// I think this is how we tell it these things are DirectInput stuff?
IDirectInputEffect* constantForceEffect = nullptr;
IDirectInputEffect* damperEffect = nullptr;
IDirectInputEffect* springEffect = nullptr;

DIJOYSTATE2 js; // No idea what this is


// === Shared Telemetry Display Data ===
struct TelemetryDisplayData {
    //GP2
    
    double gp2_structSize = 0.0;
    
    double gp2_isInRace = 0.0;
    double gp2_isPlayer = 0.0;
    double gp2_isPaused = 0.0;
    double gp2_isReplay = 0.0;
    double gp2_isX86MenuOn = 0.0;
    int gp2_deviceID = 0;

    double gp2_speedKmh = 0.0;
    double gp2_stWheelAngle = 0.0;
    double gp2_tyreTurnAngle = 0.0;
    double gp2_slipAngleFront = 0.0;
    double gp2_slipAngleRear = 0.0;

    float gp2_magLat_lf = 0.0;
    float gp2_magLat_rf = 0.0;

    float gp2_magLong_lf = 0.0;
    float gp2_magLong_rf = 0.0;

    double gp2_surfaceType_lf = 0.0;
    double gp2_surfaceType_rf = 0.0;
    double gp2_surfaceType_lr = 0.0;
    double gp2_surfaceType_rr = 0.0;

    double gp2_rideHeights_lf = 0.0;
    double gp2_rideHeights_rf = 0.0;
    double gp2_rideHeights_lr = 0.0;
    double gp2_rideHeights_rr = 0.0;

    double gp2_wheelSpin_13C_lf = 0.0;
    double gp2_wheelSpin_13C_rf = 0.0;
    double gp2_wheelSpin_13C_lr = 0.0;
    double gp2_wheelSpin_13C_rr = 0.0;

    double gp2_notOnDamper_lf = 0.0;
    double gp2_notOnDamper_rf = 0.0;
    double gp2_notOnDamper_lr = 0.0;
    double gp2_notOnDamper_rr = 0.0;

    double gp2_calc_248_lf = 0.0;
    double gp2_calc_248_rf = 0.0;
    double gp2_calc_248_lr = 0.0;
    double gp2_calc_248_rr = 0.0;

    double gp2_wheel_2AC_lf = 0.0;
    double gp2_wheel_2AC_rf = 0.0;
    double gp2_wheel_2AC_lr = 0.0;
    double gp2_wheel_2AC_rr = 0.0;


    

    // Vehicle Dynamics calculated data
    double vd_lateralG = 0.0;
    double vd_frontLeftForce_N = 0.0;
    double vd_frontRightForce_N = 0.0;
    double vd_frontLeftLong_N = 0.0;
    double vd_frontRightLong_N = 0.0;
    int vd_directionVal = 0;
    //double vd_yaw = 0.0;
    double vd_slip = 0.0;
    int vd_forceMagnitude = 0;

    // Individual tire forces
    double vd_force_lf = 0.0;
    double vd_force_rf = 0.0;
    double vd_force_lr = 0.0;
    double vd_force_rr = 0.0;

    // Aggregate forces
    double vd_frontLateralForce = 0.0;
    double vd_rearLateralForce = 0.0;
    double vd_totalLateralForce = 0.0;
    double vd_yawMoment = 0.0;
};

// === Shared Globals ===
std::mutex displayMutex;
TelemetryDisplayData displayData;
std::atomic<double> currentSpeed = 0.0;
int g_currentFFBForce = 0;
int g_currentFrontLoad = 0;

// Check Admin rights
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {

        CheckTokenMembership(NULL, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }

    return isAdmin == TRUE;
}

void RestartAsAdmin() {
    // Get the current executable path
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Use ShellExecuteW to restart with "runas" (admin prompt)
    HINSTANCE result = ShellExecuteW(
        NULL,                    // Parent window
        L"runas",               // Operation (request admin)
        exePath,                // Program to run
        NULL,                   // Command line arguments
        NULL,                   // Working directory
        SW_SHOWNORMAL           // Show window normally
    );

    // Check if the restart was successful
    if ((intptr_t)result > 32) {
        // Success - the new admin instance is starting, exit this one
        std::wcout << L"[INFO] Restarting with administrator privileges..." << std::endl;
        exit(0);
    }
    else {
        // Failed - user probably clicked "No" on UAC prompt
        std::wcout << L"[ERROR] Failed to restart as administrator." << std::endl;
        std::wcout << L"[ERROR] Please right-click the program and select 'Run as administrator'" << std::endl;
    }
}

// Console drawing stuff
// little function to help with display refreshing
// moves cursor to top without refreshing the screen

void SetConsoleWindowSize() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        LogMessage(L"[ERROR] Failed to get console handle");
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
        LogMessage(L"[ERROR] Failed to get console buffer info");
        return;
    }

    COORD bufferSize = { 120, 200 };           // scrollable size
    if (!SetConsoleScreenBufferSize(hOut, bufferSize)) {
        LogMessage(L"[WARNING] Failed to set console buffer size");
    }

    SMALL_RECT windowSize = { 0, 0, 119, 40 };  // window size (note: 119, not 120)
    if (!SetConsoleWindowInfo(hOut, TRUE, &windowSize)) {
        LogMessage(L"[WARNING] Failed to set console window size");
    }
    else {
        LogMessage(L"[INFO] Console window size set successfully");
    }
}

// Prevent lockup if window is clicked
void DisableConsoleQuickEdit() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput == INVALID_HANDLE_VALUE) {
        LogMessage(L"[ERROR] Failed to get input handle");
        return;
    }

    DWORD mode;
    if (!GetConsoleMode(hInput, &mode)) { 
        LogMessage(L"[ERROR] Failed to get console mode");
        return;
    }

 
    mode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE);
    mode |= ENABLE_EXTENDED_FLAGS;

    if (!SetConsoleMode(hInput, mode)) { 
        LogMessage(L"[ERROR] Failed to set console mode");
    }
    else {
        LogMessage(L"[INFO] Console Quick Edit Mode disabled");
    }
}

void MoveCursorToTop() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD topLeft = { 0, 0 };
    SetConsoleCursorPosition(hConsole, topLeft);
}

void MoveCursorToLine(short lineNumber) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { 0, lineNumber };
    SetConsoleCursorPosition(hConsole, pos);
}

void HideConsoleCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        LogMessage(L"[ERROR] Failed to get console handle for cursor");
        return;
    }

    CONSOLE_CURSOR_INFO cursorInfo;
    if (!GetConsoleCursorInfo(hOut, &cursorInfo)) {
        LogMessage(L"[ERROR] Failed to get cursor info");
        return;
    }

    cursorInfo.bVisible = FALSE;
    if (!SetConsoleCursorInfo(hOut, &cursorInfo)) {
        LogMessage(L"[ERROR] Failed to hide cursor");
    }
    else {
        LogMessage(L"[INFO] Cursor hidden successfully");
    }
}

// New display

void DisplayTelemetry(const TelemetryDisplayData& displayData, double masterForceValue) {
    // Move cursor to top and set up formatting
    MoveCursorToTop();
    std::cout << std::fixed << std::setprecision(2);
    std::wcout << std::fixed << std::setprecision(2);  // Also set for wide cout

    // Define console width
    const size_t CONSOLE_WIDTH = 80;

    // Helper lambda to pad lines
    auto padLine = [CONSOLE_WIDTH](const std::wstring& text) {
        std::wstring padded = text;
        if (padded.length() < CONSOLE_WIDTH) {
            padded.append(CONSOLE_WIDTH - padded.length(), L' ');
        }
        else if (padded.length() > CONSOLE_WIDTH) {
            padded = padded.substr(0, CONSOLE_WIDTH);  // Truncate if too long
        }
        return padded;
        };

    // Header section
    std::wcout << padLine(L"GP2 FFB Program Version 0.3.2 BETA") << L"\n";
    std::wcout << padLine(L"") << L"\n";
    std::wcout << padLine(L"Connected Device: " + targetDeviceName) << L"\n";
    std::wcout << padLine(L"Game: " + targetGameVersion) << L"\n";

    std::wostringstream ss;
    ss << std::fixed << std::setprecision(2);  // Set formatting for stringstream too
    ss << L"Master Force Scale: " << masterForceValue << L"%";
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";  // Empty line

    // Raw data section
    std::wcout << padLine(L"      == Raw Data ==") << L"\n";
    std::wcout << padLine(L"") << L"\n";

    /*
    // GP2 Program States
    ss.str(L""); ss.clear();
    ss << L"In Race: " << std::setw(10) << displayData.gp2_isInRace;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Is Player: " << std::setw(10) << displayData.gp2_isPlayer;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Is Paused: " << std::setw(10) << displayData.gp2_isPaused;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Is Replay: " << std::setw(10) << displayData.gp2_isReplay;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"x86 Menu: " << std::setw(10) << displayData.gp2_isX86MenuOn;
    std::wcout << padLine(ss.str()) << L"\n";
    */

    std::wcout << padLine(L"") << L"\n";  // Empty line


   // ss.str(L""); ss.clear();
   // ss << L"Device Name: " << std::setw(10) << displayData.gp2_deviceID;
   // std::wcout << padLine(ss.str()) << L"\n";


    ss.str(L""); ss.clear();
    ss << L"Speed: " << std::setw(8) << displayData.gp2_speedKmh << L" kph";
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Steering Wheel Angle: " << std::setw(10) << displayData.gp2_stWheelAngle;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Tyre Turn Angle: " << std::setw(10) << displayData.gp2_tyreTurnAngle;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Slip Angle Front: " << std::setw(10) << displayData.gp2_slipAngleFront;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Slip Angle Rear: " << std::setw(10) << displayData.gp2_slipAngleRear;
    std::wcout << padLine(ss.str()) << L"\n";


    std::wcout << padLine(L"") << L"\n";  // Empty line

    // Tire loads section
    std::wcout << padLine(L"      == Tire/Suspension Data ==") << L"\n";
    std::wcout << padLine(L"") << L"\n";
    std::wcout << padLine(L"      Left Front      Right Front") << L"\n";


    //Raw Forces
    /*
    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Mag Lat: " << static_cast<int>(displayData.gp2_magLat_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_magLat_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Mag Long: " << static_cast<int>(displayData.gp2_magLong_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_magLong_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";
    */

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Mag Lat: " << static_cast<int>(displayData.vd_frontLeftForce_N) << L"           " << std::setw(10) << static_cast<int>(displayData.vd_frontRightForce_N);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Mag Long: " << static_cast<int>(displayData.vd_frontLeftLong_N) << L"           " << std::setw(10) << static_cast<int>(displayData.vd_frontRightLong_N);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Surface: " << static_cast<int>(displayData.gp2_surfaceType_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_surfaceType_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";
    /*
    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Ride Height: " << static_cast<int>(std::abs(displayData.gp2_rideHeights_lf)) << L"           " << std::setw(10) << static_cast<int>(std::abs(displayData.gp2_rideHeights_rf));
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Wheel Spin: " << static_cast<int>(displayData.gp2_wheelSpin_13C_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_wheelSpin_13C_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Not on Damper: " << static_cast<int>(displayData.gp2_notOnDamper_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_notOnDamper_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Calc 248: " << static_cast<int>(displayData.gp2_calc_248_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_calc_248_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Wheel 2AC: " << static_cast<int>(displayData.gp2_wheel_2AC_lf) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_wheel_2AC_rf);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    */

    //ss.str(L""); ss.clear();
    //ss << std::setw(10) << L"latN: " << static_cast<int16_t>(displayData.vd_frontLeftForce_N) << L"           " << std::setw(10) << static_cast<int16_t>(displayData.vd_frontRightForce_N);
    //std::wcout << padLine(ss.str()) << L"\n";
    //std::wcout << padLine(L"") << L"\n";
  
   // std::wcout << padLine(L"      Left Rear      Right Rear") << L"\n";
    //ss.str(L""); ss.clear();
    //ss << std::setw(10) << displayData.tireload_lr << L"           " << std::setw(10) << displayData.tireload_rr;
    //std::wcout << padLine(ss.str()) << L"\n";
    //std::wcout << padLine(L"") << L"\n";



    //ss.str(L""); ss.clear();
    //ss << std::setw(10) << L"Surface: " << static_cast<int>(displayData.gp2_surfaceType_lr) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_surfaceType_rr);
    //std::wcout << padLine(ss.str()) << L"\n";
    //std::wcout << padLine(L"") << L"\n";
  /*
    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Ride Height: " << static_cast<int>(displayData.gp2_rideHeights_lr) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_rideHeights_rr);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Wheel Spin: " << static_cast<int>(displayData.gp2_wheelSpin_13C_lr) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_wheelSpin_13C_rr);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Not on Damper: " << static_cast<int>(displayData.gp2_notOnDamper_lr) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_notOnDamper_rr);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Calc 248: " << static_cast<int>(displayData.gp2_calc_248_lr) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_calc_248_rr);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << L"Wheel 2AC: " << static_cast<int>(displayData.gp2_wheel_2AC_lr) << L"           " << std::setw(10) << static_cast<int>(displayData.gp2_wheel_2AC_rr);
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";
*/

    // Vehicle Dynamics section
    std::wcout << padLine(L"      == Vehicle Dynamics ==") << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Lateral G: " << std::setw(8) << displayData.vd_lateralG << L" G";
    std::wcout << padLine(ss.str()) << L"\n";

    //ss.str(L""); ss.clear();
    //ss << L"Yaw Rate: " << std::setw(8) << displayData.vd_yaw << L" deg/s²";
    //std::wcout << padLine(ss.str()) << L"\n";

    //ss.str(L""); ss.clear();
    //ss << L"Longi Force: " << std::setw(8) << displayData.long_force << L"";
    //std::wcout << padLine(ss.str()) << L"\n";

    //ss.str(L""); ss.clear();
    //ss << L"Direction Value: " << displayData.vd_directionVal;
    //std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Front Force Calc: " << std::setw(10) << g_currentFrontLoad;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Force Magnitude: " << g_currentFFBForce;
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    /*
    // Tire Forces
    std::wcout << padLine(L"      == Decoded Tire Forces ==") << L"\n";
    std::wcout << padLine(L"") << L"\n";
    std::wcout << padLine(L"Front Left      Front Right") << L"\n";

    ss.str(L""); ss.clear();
    ss << std::setw(10) << displayData.vd_force_lf << L"           " << std::setw(10) << displayData.vd_force_rf;
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    std::wcout << padLine(L"Rear Left       Rear Right") << L"\n";
    ss.str(L""); ss.clear();
    ss << std::setw(10) << displayData.vd_force_lr << L"           " << std::setw(10) << displayData.vd_force_rr;
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Front Total: " << std::setw(8) << displayData.vd_frontLateralForce << L"   Rear Total: " << std::setw(8) << displayData.vd_rearLateralForce;
    std::wcout << padLine(ss.str()) << L"\n";

    ss.str(L""); ss.clear();
    ss << L"Total Force: " << std::setw(8) << displayData.vd_totalLateralForce << L"   Yaw Moment: " << std::setw(8) << displayData.vd_yawMoment;
    std::wcout << padLine(ss.str()) << L"\n";
    std::wcout << padLine(L"") << L"\n";

    /*
    */
    

    //ss.str(L""); ss.clear();
    //ss << L"Force Magnitude: " << displayData.forceMagnitude;
    //std::wcout << padLine(ss.str()) << L"\n";

    std::wcout << padLine(L"----------------------------------------") << L"\n";
    std::wcout << padLine(L"Log:") << L"\n";
}

// Logging stuff - Keeps messages for future debugging!
// Write to log.txt
void LogMessage(const std::wstring& msg) {
    std::lock_guard<std::mutex> lock(logMutex);

    // Add to in-memory deque for optional UI display (if needed)
    logLines.push_back(msg);
    if (logLines.size() > maxLogLines)
        logLines.pop_front();

    // Append to log.txt
    std::wofstream logFile("log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << msg << std::endl;
    }
}

// === Force Effect Creators ===
void CreateConstantForceEffect(LPDIRECTINPUTDEVICE8 device) {
    if (!device) return;

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

    DIPROPRANGE diprg = {};
    diprg.diph.dwSize = sizeof(DIPROPRANGE);
    diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    diprg.diph.dwHow = DIPH_BYOFFSET;
    diprg.diph.dwObj = DIJOFS_X;
    diprg.lMin = -10000;
    diprg.lMax = 10000;
    matchedDevice->SetProperty(DIPROP_RANGE, &diprg.diph);

    HRESULT hr = device->CreateEffect(GUID_ConstantForce, &eff, &constantForceEffect, nullptr);
    if (FAILED(hr)) {
        LogMessage(L"[ERROR] Failed to create constant force effect. HRESULT: 0x" + std::to_wstring(hr));
    }
    else {
        LogMessage(L"[INFO] Initial constant force created");
    }
}

void CreateDamperEffect(IDirectInputDevice8* device) {
    if (!device) return;

    DICONDITION condition = {};
    condition.lOffset = 0;
    condition.lPositiveCoefficient = 8000;
    condition.lNegativeCoefficient = 8000;
    condition.dwPositiveSaturation = 10000;
    condition.dwNegativeSaturation = 10000;
    condition.lDeadBand = 0;

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
    eff.cbTypeSpecificParams = sizeof(DICONDITION);
    eff.lpvTypeSpecificParams = &condition;

    HRESULT hr = device->CreateEffect(GUID_Damper, &eff, &damperEffect, nullptr);
    if (FAILED(hr) || !damperEffect) {
        LogMessage(L"[ERROR] Failed to create damper effect. HRESULT: 0x" + std::to_wstring(hr));
    }
    else {
        LogMessage(L"[INFO] Initial damper effect created");
    }
}

void CreateSpringEffect(IDirectInputDevice8* device) {
    if (!device) return;

    DICONDITION condition = {};
    condition.lOffset = 0;
    condition.lPositiveCoefficient = 8000;
    condition.lNegativeCoefficient = 8000;
    condition.dwPositiveSaturation = 10000;
    condition.dwNegativeSaturation = 10000;
    condition.lDeadBand = 0;

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
    eff.cbTypeSpecificParams = sizeof(DICONDITION);
    eff.lpvTypeSpecificParams = &condition;

    HRESULT hr = device->CreateEffect(GUID_Spring, &eff, &springEffect, nullptr);
    if (FAILED(hr) || !springEffect) {
        LogMessage(L"[ERROR] Failed to create spring effect. HRESULT: 0x" + std::to_wstring(hr));
    }
    else {
        LogMessage(L"[INFO] Initial spring effect created");
    }
}

// Loop which kicks stuff off and coordinates everything!
void ProcessLoop() {
   
    //Get some data from RawTelemetry -> not 100% sure what this does
    RawTelemetry current{};
    RawTelemetry previousVD{};
    bool firstReadingVD = true;

    double previousDlong = 0.0;
    int noMovementFrames = 0;
    const int movementThreshold = 3;  // number of frames to consider "stopped"
    bool effectPaused = false;

    //Added for feedback skipping if stopped
    RawTelemetry previousPos{};
    bool firstPos = true;

    static bool versionChecked = false;  // Only check once

    while (true) {
        double currentTime = getPerformanceCounterTime();

        // Check to see if Telemetry is coming in, but if not then wait for it!
        if (!ReadTelemetryData(current)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        static int versionCheckAttempts = 0;

        if (!versionChecked) {
            if (current.gp2_structSize == 0) {
                // Game hasn't fully initialized yet, keep waiting
                versionCheckAttempts++;
                if (versionCheckAttempts % 100 == 0) { // Log every ~1.5 seconds at 60fps
                    LogMessage(L"[INFO] Waiting for x86GP2 to initialize... (attempt " + std::to_wstring(versionCheckAttempts) + L")");
                }
                continue; // Skip to next loop iteration
            }
            else if (current.gp2_structSize != 2720) {
                LogMessage(L"[ERROR] Wrong version of x86GP2 detected");
                LogMessage(L"[ERROR] Expected struct size: 2720, Got: " + std::to_wstring(current.gp2_structSize));
                LogMessage(L"[ERROR] This is the wrong version of x86GP2, please update and try again.");
                std::cin.get();
                exit(1);
            }
            else {
                // Valid version detected
                versionChecked = true;
                LogMessage(L"[INFO] x86GP2 version check passed - struct size: " + std::to_wstring(current.gp2_structSize));
                LogMessage(L"[INFO] Version check completed after " + std::to_wstring(versionCheckAttempts) + L" attempts");
            }
        }


        if (currentTime >= FFBTime) {

            if (firstPos) { previousPos = current; firstPos = false; }


            // Start damper/spring effects once telemetry is valid
            // Probably need to also figure out how to stop these when the game pauses
            // Also need to maybe fade in and out the effects when waking/sleeping
            if (!damperStarted && damperEffect && enableDamperEffect) {
                damperEffect->Start(1, 0);
                damperStarted = true;
                LogMessage(L"[INFO] Damper effect started");
            }

            if (!springStarted && springEffect && enableSpringEffect) {
                springEffect->Start(1, 0);
                springStarted = true;
                LogMessage(L"[INFO] Spring effect started");
            }

            // Master force scale -> Keeping Hands Safe
            double masterForceValue = std::stod(targetForceSetting);
            double masterForceScale = std::clamp(masterForceValue / 100.0, 0.0, 1.0);

            double deadzoneForceValue = std::stod(targetDeadzoneSetting);
            double deadzoneForceScale = std::clamp(deadzoneForceValue / 100.0, 0.0, 1.0);

            double constantForceValue = std::stod(targetConstantScale);
            double constantForceScale = std::clamp(constantForceValue / 100.0, 0.0, 1.0);

            double brakingForceValue = std::stod(targetBrakingScale);
            double brakingForceScale = brakingForceValue;

            double weightForceValue = std::stod(targetWeightScale);
            double weightForceScale = std::clamp(weightForceValue / 100.0, 0.0, 1.0);

            double damperForceValue = std::stod(targetDamperScale);
            double damperForceScale = std::clamp(damperForceValue / 100.0, 0.0, 1.0);

            // Update Effects
            if (damperEffect && enableDamperEffect)
                UpdateDamperEffect(current.gp2_speedKmh, damperEffect, masterForceScale, damperForceScale);

            if (springEffect && enableSpringEffect)
                UpdateSpringEffect(springEffect, masterForceScale);


            CalculatedVehicleDynamics vehicleDynamics{};
            bool vehicleDynamicsValid = CalculateVehicleDynamics(current, previousVD, firstReadingVD, vehicleDynamics);


        if (vehicleDynamicsValid) {
            if (FAILED(matchedDevice->Poll())) {
                    matchedDevice->Acquire();
                    matchedDevice->Poll();
                }
                matchedDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js);

                // Start constant force once telemetry is valid 
                if (enableConstantForce && constantForceEffect) {
                    if (!constantStarted) {
                        constantForceEffect->Start(1, 0);
                        constantStarted = true;
                        LogMessage(L"[INFO] Constant force started");
                    }

                    //This is what will add the "Constant Force" effect if all the calculations work. 
                    // Probably could smooth all this out
                    ApplyConstantForceEffect(current,
                        vehicleDynamics, current.gp2_speedKmh, constantForceEffect, enableWeightForce, enableRateLimit,
                        masterForceScale, deadzoneForceScale,
                        constantForceScale, brakingForceScale, weightForceScale);
                    previousPos = current;

                }


                //Setting variables for next update
                currentSpeed = current.gp2_speedKmh;


                // Update telemetry for display
                {
                    std::lock_guard<std::mutex> lock(displayMutex);

                    //GP2 Telemetry

                    displayData.gp2_isInRace = current.gp2_structSize;


                    displayData.gp2_isInRace = current.gp2_isInRace;
                    displayData.gp2_isPlayer = current.gp2_isPlayer;
                    displayData.gp2_isPaused = current.gp2_isPaused;
                    displayData.gp2_isReplay = current.gp2_isReplay;
                    displayData.gp2_isX86MenuOn = current.gp2_isX86MenuOn;
                    displayData.gp2_deviceID = current.gp2_deviceID;

                    displayData.gp2_speedKmh = current.gp2_speedKmh;
                    displayData.gp2_stWheelAngle = current.gp2_stWheelAngle;
                    displayData.gp2_tyreTurnAngle = current.gp2_tyreTurnAngle;
                    displayData.gp2_slipAngleFront = current.gp2_slipAngleFront;
                    displayData.gp2_slipAngleRear = current.gp2_slipAngleRear;

                    displayData.gp2_magLat_lf = current.gp2_magLat_lf;
                    displayData.gp2_magLat_rf = current.gp2_magLat_rf;

                    displayData.gp2_magLong_lf = current.gp2_magLong_lf;
                    displayData.gp2_magLong_rf = current.gp2_magLong_rf;

                    displayData.gp2_surfaceType_lf = current.gp2_surfaceType_lf;
                    displayData.gp2_surfaceType_rf = current.gp2_surfaceType_rf;
                    displayData.gp2_surfaceType_lr = current.gp2_surfaceType_lr;
                    displayData.gp2_surfaceType_rr = current.gp2_surfaceType_rr;

                    displayData.gp2_rideHeights_lf = current.gp2_rideHeights_lf;
                    displayData.gp2_rideHeights_rf = current.gp2_rideHeights_rf;
                    displayData.gp2_rideHeights_lr = current.gp2_rideHeights_lr;
                    displayData.gp2_rideHeights_rr = current.gp2_rideHeights_rr;

                    displayData.gp2_wheelSpin_13C_lf = current.gp2_wheelSpin_13C_lf;
                    displayData.gp2_wheelSpin_13C_rf = current.gp2_wheelSpin_13C_rf;
                    displayData.gp2_wheelSpin_13C_lr = current.gp2_wheelSpin_13C_lr;
                    displayData.gp2_wheelSpin_13C_rr = current.gp2_wheelSpin_13C_rr;

                    displayData.gp2_notOnDamper_lf = current.gp2_notOnDamper_lf;
                    displayData.gp2_notOnDamper_rf = current.gp2_notOnDamper_rf;
                    displayData.gp2_notOnDamper_lr = current.gp2_notOnDamper_lr;
                    displayData.gp2_notOnDamper_rr = current.gp2_notOnDamper_rr;

                    displayData.gp2_calc_248_lf = current.gp2_calc_248_lf;
                    displayData.gp2_calc_248_rf = current.gp2_calc_248_rf;
                    displayData.gp2_calc_248_lr = current.gp2_calc_248_lr;
                    displayData.gp2_calc_248_rr = current.gp2_calc_248_rr;

                    displayData.gp2_wheel_2AC_lf = current.gp2_wheel_2AC_lf;
                    displayData.gp2_wheel_2AC_rf = current.gp2_wheel_2AC_rf;
                    displayData.gp2_wheel_2AC_lr = current.gp2_wheel_2AC_lr;
                    displayData.gp2_wheel_2AC_rr = current.gp2_wheel_2AC_rr;


                    // NEW: Vehicle dynamics data (only update if calculation was successful)
                    if (vehicleDynamicsValid) {
                        displayData.vd_lateralG = vehicleDynamics.lateralG;
                        displayData.vd_directionVal = vehicleDynamics.directionVal;
                        displayData.vd_frontLeftForce_N = vehicleDynamics.frontLeftForce_N;
                        displayData.vd_frontRightForce_N = vehicleDynamics.frontRightForce_N;
                        displayData.vd_frontLeftLong_N = vehicleDynamics.frontLeftLong_N;
                        displayData.vd_frontRightLong_N = vehicleDynamics.frontRightLong_N;
                        //displayData.vd_yaw = vehicleDynamics.yaw;
                        displayData.vd_slip = vehicleDynamics.slip;
                        displayData.vd_forceMagnitude = vehicleDynamics.forceMagnitude;

                        // Individual tire forces
                        displayData.vd_force_lf = vehicleDynamics.force_lf;
                        displayData.vd_force_rf = vehicleDynamics.force_rf;
                        displayData.vd_force_lr = vehicleDynamics.force_lr;
                        displayData.vd_force_rr = vehicleDynamics.force_rr;

                        // Aggregate forces
                        displayData.vd_frontLateralForce = vehicleDynamics.frontLateralForce;
                        displayData.vd_rearLateralForce = vehicleDynamics.rearLateralForce;
                        displayData.vd_totalLateralForce = vehicleDynamics.totalLateralForce;
                        displayData.vd_yawMoment = vehicleDynamics.yawMoment;
                    }
                }
            }
            FFBTime += FFB_INTERVAL;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// Where it all happens
int main() {
    
    if (!IsRunningAsAdmin()) {
        std::wcout << L"===============================================" << std::endl;
        std::wcout << L"    GP2 FFB Program - Admin Rights Required" << std::endl;
        std::wcout << L"===============================================" << std::endl;
        std::wcout << L"" << std::endl;
        std::wcout << L"This program requires administrator privileges to:" << std::endl;
        std::wcout << L"  - Access DirectInput force feedback devices" << std::endl;
        std::wcout << L"  - Control console display properly" << std::endl;
        std::wcout << L"  - Ensure reliable wheel communication" << std::endl;
        std::wcout << L"" << std::endl;
        std::wcout << L"Would you like to restart as administrator? (y/n): ";

        wchar_t response;
        std::wcin >> response;

        if (response == L'y' || response == L'Y') {
            RestartAsAdmin();
            // If we get here, the restart failed
            std::wcout << L"Press any key to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        else {
            std::wcout << L"" << std::endl;
            std::wcout << L"Cannot continue without administrator privileges." << std::endl;
            std::wcout << L"Please restart the program as administrator." << std::endl;
            std::wcout << L"Press any key to exit..." << std::endl;
            std::wcin.ignore();
            std::wcin.get();
            return 1;
        }
    }

    SetConsoleWindowSize();
    HideConsoleCursor();
    DisableConsoleQuickEdit();

    //timing
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    //clear last log
    std::wofstream clearLog("log.txt", std::ios::trunc);


    // Load FFB configuration file "ffb.ini"
    if (!LoadFFBSettings(L"ffb.ini")) {
        LogMessage(L"[ERROR] Failed to load FFB settings from ffb.ini");
        LogMessage(L"[ERROR] Make sure ffb.ini exists and has proper format");

        // SHOW ERROR ON CONSOLE immediately
        std::wcout << L"[ERROR] Failed to load FFB settings from ffb.ini" << std::endl;
        std::wcout << L"[ERROR] Make sure ffb.ini exists and has proper format" << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    LogMessage(L"[INFO] Successfully loaded FFB settings");
    LogMessage(L"[INFO] Target device: " + targetDeviceName);

    // Initialize DirectInput device
    // I don't really know how this works yet. It enabled "exclusive" mode on the device and calls it a "type 2" joystick.. OK!
    if (!InitializeDevice()) {
        LogMessage(L"[ERROR] Failed to initialize DirectInput or find device: " + targetDeviceName);
        LogMessage(L"[ERROR] Available devices:");

        // List available devices to help user
        ListAvailableDevices();

        LogMessage(L"[ERROR] Check your ffb.ini file - device name must match exactly");

        // SHOW ERROR ON CONSOLE immediately
        std::wcout << L"[ERROR] Could not find controller: " << targetDeviceName << std::endl;
        std::wcout << L"[ERROR] Available devices:" << std::endl;

        // Show available devices on console too
        ShowAvailableDevicesOnConsole();

        std::wcout << L"[ERROR] Check your ffb.ini file - device name must match exactly" << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    LogMessage(L"[INFO] Device found and initialized successfully");


    // ONLY configure device if it was found successfully
    HRESULT hr;
    hr = matchedDevice->SetDataFormat(&c_dfDIJoystick2);
    if (FAILED(hr)) {
        LogMessage(L"[ERROR] Failed to set data format: 0x" + std::to_wstring(hr));
        
        std::wcout << L"[ERROR] Failed to set data format: 0x" << std::hex << hr << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    hr = matchedDevice->SetCooperativeLevel(GetConsoleWindow(), DISCL_BACKGROUND | DISCL_EXCLUSIVE);
    if (FAILED(hr)) {
        LogMessage(L"[ERROR] Failed to set cooperative level: 0x" + std::to_wstring(hr));
        LogMessage(L"[ERROR] Another application may be using the device exclusively");
        
        std::wcout << L"[ERROR] Failed to set cooperative level: 0x" << std::hex << hr << std::endl;
        std::wcout << L"[ERROR] Another application may be using the device exclusively" << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    hr = matchedDevice->Acquire();  
    if (FAILED(hr)) {
        LogMessage(L"[WARNING] Initial acquire failed: 0x" + std::to_wstring(hr) + L" (this is often normal)");
    }
    else {
        LogMessage(L"[INFO] Device acquired successfully");
    }

    // Parse FFB effect toggles from config <- should all ffb types be enabled? Allows user to select if they dont like damper for instance
    // Would be nice to add a % per effect in the future
    enableRateLimit = (targetWeightEnabled == L"true" || targetWeightEnabled == L"True");
    enableConstantForce = (targetConstantEnabled == L"true" || targetConstantEnabled == L"True");
    enableWeightForce = (targetWeightEnabled == L"true" || targetWeightEnabled == L"True");
    enableDamperEffect = (targetDamperEnabled == L"true" || targetDamperEnabled == L"True");
    enableSpringEffect = (targetSpringEnabled == L"true" || targetSpringEnabled == L"True");

    // Create FFB effects as needed
    if (enableConstantForce) CreateConstantForceEffect(matchedDevice);
    if (enableDamperEffect)  CreateDamperEffect(matchedDevice);
    if (enableSpringEffect)  CreateSpringEffect(matchedDevice);

    // This is to control the max % for any of the FFB effects as specified in the ffb.ini
    // Prevents broken wrists (hopefully)
    double masterForceValue = std::stod(targetForceSetting);
    double constantForceValue = std::stod(targetConstantScale);
    double weightForceValue = std::stod(targetWeightScale);
    double damperForceValue = std::stod(targetDamperScale);

    // Start telemetry processing!
    std::thread processThread(ProcessLoop);
    processThread.detach();

    // Now that we're doing everything we can display stuff!
    // Main Display Loop - Set to 200ms? Probably fine
    // Flickers a lot right now but perhaps moving to a GUI will solve that eventually
    while (true) {
        double currentTime = getPerformanceCounterTime();

        if (currentTime >= printTime) {
            // make sure we stay at most recent display update
            MoveCursorToLine(0);

            //Trigger display
            {
                std::lock_guard<std::mutex> lock(displayMutex);
                DisplayTelemetry(displayData, masterForceValue);
            }

            //Print log data
            {
                std::lock_guard<std::mutex> lock(logMutex);
                size_t maxDisplayLines = 1; //how many lines to display
                std::vector<std::wstring> recentUniqueLines;
                std::unordered_set<std::wstring> seen;

                // Go backward to find most recent unique messages
                for (auto it = logLines.rbegin(); it != logLines.rend() && recentUniqueLines.size() < maxDisplayLines; ++it) {
                    if (seen.insert(*it).second) {
                        recentUniqueLines.push_back(*it);
                    }
                }

                // Reverse to show most recent at bottom
                std::reverse(recentUniqueLines.begin(), recentUniqueLines.end());

                for (const auto& line : recentUniqueLines) {
                    std::wstring padded = line;
                    padded.resize(80, L' ');  // pad to 80 characters to clear old line leftovers
                    std::wcout << padded << L"\n";
                }
            }
            printTime = currentTime + PRINT_INTERVAL;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}
