#pragma once
#include <dinput.h>
#include <string>

// Include logging
void LogMessage(const std::wstring& msg);

extern IDirectInputEffect* damperEffect;

void UpdateDamperEffect(double gp2_speedKmh, IDirectInputEffect* effect, double masterForceScale, double damperForceScale);