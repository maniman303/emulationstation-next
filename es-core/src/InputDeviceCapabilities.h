#pragma once

#include <string>

inline bool hasPhysicalRightAnalogStick(const std::string& model, const std::string& deviceName,
	const std::string& deviceSysfsPath)
{
	// The single-stick RG DS Plus is exposed by InputPlumber as a dual-stick DS5.
	// Its unused ADC channels mirror the left stick. Physical external controllers
	// must retain their own right-stick capability, even on this handheld.
	return model != "Anbernic RG DS Plus" ||
		deviceName != "Sony Interactive Entertainment DualSense Wireless Controller" ||
		deviceSysfsPath.find("/sys/devices/virtual/misc/uhid/") != 0;
}
