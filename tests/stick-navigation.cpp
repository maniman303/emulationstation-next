#include "InputConfig.h"
#include "InputDeviceCapabilities.h"
#include <cstdlib>
#include <iostream>

static void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    InputConfig config(0, 0, "Controller", "test", 12, 1, 6);
    const std::string dualSense = "Sony Interactive Entertainment DualSense Wireless Controller";
    const std::string virtualPath = "/sys/devices/virtual/misc/uhid/0003:054C:0CE6.0002/input/input17";
    check(!hasPhysicalRightAnalogStick("Anbernic RG DS Plus", dualSense, virtualPath), "Built-in phantom right stick must be unavailable");
    check(hasPhysicalRightAnalogStick("Anbernic RG DS Plus", dualSense, "/sys/devices/platform/usb/input/input20"), "External DualSense must keep its real right stick");
    check(hasPhysicalRightAnalogStick("Anbernic RG DS", dualSense, virtualPath), "Dual-stick handheld must retain its right stick");
    check(hasPhysicalRightAnalogStick("", dualSense, virtualPath), "Unknown hardware must keep existing behavior");
    check(hasPhysicalRightAnalogStick("Anbernic RG DS Plus", "External controller", virtualPath), "Other controllers must retain their right stick");
    config.setRightAnalogAvailable(false);
    const char* directions[] = {"left", "right", "up", "down"};
    const char* left[] = {"leftanalogleft", "leftanalogright", "leftanalogup", "leftanalogdown"};
    const char* right[] = {"rightanalogleft", "rightanalogright", "rightanalogup", "rightanalogdown"};
    const int axes[] = {0, 0, 1, 1};
    const int signs[] = {-1, 1, -1, 1};
    const int hats[] = {SDL_HAT_LEFT, SDL_HAT_RIGHT, SDL_HAT_UP, SDL_HAT_DOWN};
    for (int i = 0; i < 4; ++i)
    {
        config.mapInput(directions[i], Input(0, TYPE_HAT, 0, hats[i], true));
        config.mapInput(left[i], Input(0, TYPE_AXIS, axes[i], signs[i], true));
        config.mapInput(right[i], Input(0, TYPE_AXIS, axes[i] + 3, signs[i], true));
    }
    for (int i = 0; i < 4; ++i)
    {
        Input stick(0, TYPE_AXIS, axes[i], signs[i], false);
        Input mirrored(0, TYPE_AXIS, axes[i] == 0 ? 4 : 3, axes[i] == 0 ? signs[i] : -signs[i], false);
        int matches = 0;
        for (const auto* direction : directions)
            matches += config.isMappedLike(direction, stick) + config.isMappedLike(direction, mirrored);
        check(matches == 1, "A left-stick push plus its mirrored right-stick event must yield one menu direction");
        check(config.isMappedLike(directions[i], stick), "Left-stick direction must work");
        check(config.isMappedLike(directions[i], Input(0, TYPE_HAT, 0, hats[i], false)), "D-pad direction must work");
        check(config.isMappedTo(right[i], Input(0, TYPE_AXIS, axes[i] + 3, signs[i], false)), "Explicit right-stick mappings must remain accessible");
    }
    check(!config.isMappedLike("left", Input(0, TYPE_AXIS, 0, 1, false)), "Opposite direction must not match");
    config.setRightAnalogAvailable(true);
    for (int i = 0; i < 4; ++i)
        check(config.isMappedLike(directions[i], Input(0, TYPE_AXIS, axes[i] + 3, signs[i], false)), "A real right stick must navigate in every direction");
    std::cout << "Stick navigation regression checks passed\n";
}
