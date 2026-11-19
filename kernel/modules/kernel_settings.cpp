// The kernel settings module (spec/oxos_internal/43): OXOS's own options from C:\OXOS\oxos_settings.ini, read with the shared ini reader
// (shared/ini_file/settings_ini.cpp): the boot video's switches (spec/oxos_internal/38 §2), which the kernel reads at a cold boot and keeps, and the console
// settings (spec/oxos_internal/44), applied at every launch.
#include "modules.h"
#include <diagnostics/diagnostics.h>
#include <hardware/chipset/chipset.h>
#include <in_game_reset/in_game_reset.h>
#include <ini_file/settings_ini.h>
#include <memory/memory.h>
#include <titles/titles.h>

namespace xk::modules {

namespace {

constexpr char SETTINGS_FILE[] = "\\Device\\Harddisk0\\Partition2\\OXOS\\oxos_settings.ini";
constexpr ULONG MAX_SETTINGS_FILE = 0x10000;
constexpr ULONG POOL_TAG = 'sSeX';
constexpr UCHAR SMC_ADDRESS = 0x20, SMC_FAN_MODE = 0x05, SMC_FAN_SPEED = 0x06; // the SMC; its fan registers (0x05: 1 manual, 0 automatic; 0x06: 0-50)

} // namespace

ULONG ReadBootVideoSwitches() {
    ULONG got;
    NTSTATUS status;
    char *text = titles::ReadOXOSFile(SETTINGS_FILE, MAX_SETTINGS_FILE, POOL_TAG, &got, &status);
    if (!text) {
        XK_SERIAL("settings: oxos_settings.ini not read (%#lx)\n", status);
        return 0;
    }
    BOOT_VIDEO_SETTINGS settings;
    uint32_t errors;
    oxos::ini_file::ReadBootVideoSettings(text, got, &settings, &errors);
    ExFreePool(text);
    if (errors)
        XK_SERIAL("settings: oxos_settings.ini: %u lines not understood\n", errors);
    XK_SERIAL("settings: oxos_settings.ini (%lu bytes): boot video %s, always %s\n", got, settings.Enabled ? "on" : "off", settings.Always ? "yes" : "no");
    if (!settings.Enabled)
        return 0;
    return XK_BOOT_VIDEO_ENABLED | (settings.Always ? XK_BOOT_VIDEO_ALWAYS : 0);
}

namespace {

// One SMC register write, up to five tries (the SMBus may be busy).
void WriteSmc(UCHAR Register, UCHAR Value) {
    for (int i = 0; i < 5 && !NT_SUCCESS(HalWriteSMBusValue(SMC_ADDRESS, Register, FALSE, Value)); i++) {
        // Retry until the SMC accepts the write (up to five tries).
    }
}

} // namespace

UCHAR ApplyConsoleSettings() {
    CONSOLE_SETTINGS settings;
    uint32_t errors = 0;
    ULONG got;
    NTSTATUS status;
    char *text = titles::ReadOXOSFile(SETTINGS_FILE, MAX_SETTINGS_FILE, POOL_TAG, &got, &status);
    oxos::ini_file::ReadConsoleSettings(text ? text : "", text ? got : 0, &settings, &errors);
    if (text)
        ExFreePool(text);
    if (errors)
        XK_SERIAL("settings: [console], [in_game_reset]: %u values not used\n", errors);
    XK_SERIAL("settings: console: led %#x, fan %d, reset buttons %#x, power-off buttons %#x\n", settings.FrontLed, settings.FanSpeed, settings.ResetButtons,
              settings.PowerOffButtons);
    if (settings.FanSpeed != CONSOLE_FAN_UNCHANGED) {
        WriteSmc(SMC_FAN_MODE, settings.FanSpeed == CONSOLE_FAN_AUTOMATIC ? 0 : 1);
        if (settings.FanSpeed != CONSOLE_FAN_AUTOMATIC)
            WriteSmc(SMC_FAN_SPEED, static_cast<UCHAR>(settings.FanSpeed / 2));
    }
    in_game_reset::SetButtons(settings.ResetButtons, settings.PowerOffButtons);
    return settings.FrontLed;
}

} // namespace xk::modules
