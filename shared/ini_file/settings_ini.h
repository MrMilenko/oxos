// C:\OXOS\oxos_settings.ini, the kernel's own options (spec/oxos_internal/19, 38 §1, 39): the parser. It makes no kernel calls, so the host unit tests run
// it unchanged (tests/unit/test_settings_ini.cpp), and the boot video player (apps/oxos_boot_video) reads the file with it too.
//
//   [dashboard]
//   dashboard_1 = C:\evoxdash.xbe     the dashboards to start, tried in order (dashboard_1 to dashboard_8); then C:\xboxdash.xbe
//   dashboard_2 = C:\Dashboard\default.xbe
//
//   [boot_video]
//   boot_video_enabled = yes          yes: a boot video plays; no or missing: none
//   boot_video_file = intro.mpg       a file in C:\OXOS\boot_videos\, or a full path; quotes around it are allowed
//   boot_video_always = no            yes: after every reboot that starts the dashboard or the disc; no or missing: after a cold boot only
//
//   [console]                         (spec/oxos_internal/44)
//   front_led = GGGG                  the front LED once a title starts: 4 steps, each G, R, A (amber) or - (off)
//   fan_speed = auto                  20 to 100 (percent), or auto; missing: the fan is left as it is
//
//   [in_game_reset]
//   reset_buttons = LT RT BACK BLACK  at least 3 buttons held together power-cycle the console; none: off
//   power_off_buttons = none          ... turn it off; must not overlap reset_buttons
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <ini_file/launch_redirect_ini.h>

// The dashboards [dashboard] may list: dashboard_1 to dashboard_8.
constexpr uint32_t INI_MAX_DASHBOARDS = 8;

// The longest boot_video_file kept, without its terminating zero; a longer one is taken as missing.
constexpr uint32_t BOOT_VIDEO_MAX_FILE = 259;

// The [boot_video] section.
struct BOOT_VIDEO_SETTINGS {
    bool Enabled;                     // boot_video_enabled = yes
    bool Always;                      // boot_video_always = yes
    char File[BOOT_VIDEO_MAX_FILE + 1];  // boot_video_file without quotes; empty when it is missing or too long
};

// [console] and [in_game_reset] (spec/oxos_internal/44).
constexpr uint8_t CONSOLE_LED_SOLID_GREEN = 0x0F;      // front_led = GGGG: the LED pattern as shipped
constexpr uint16_t CONSOLE_RESET_BUTTONS = 0xD020;     // LT RT BACK BLACK, in spec/oxos_internal/22 §3's button mask
constexpr int8_t CONSOLE_FAN_UNCHANGED = -1;           // fan_speed missing: OXOS leaves the fan alone
constexpr int8_t CONSOLE_FAN_AUTOMATIC = 0;            // fan_speed = auto: the SMC's own control
constexpr int8_t CONSOLE_FAN_SLOWEST = 20;             // the lowest fan_speed accepted (percent)
constexpr uint32_t CONSOLE_MIN_COMBINATION_BUTTONS = 3; // the fewest buttons an in-game reset combination may have

struct CONSOLE_SETTINGS {
    uint8_t FrontLed;        // the SMC LED pattern: bits 3-0 green, 7-4 red, one per quarter of the cycle
    int8_t FanSpeed;         // CONSOLE_FAN_UNCHANGED, CONSOLE_FAN_AUTOMATIC, or CONSOLE_FAN_SLOWEST to 100
    uint16_t ResetButtons;   // the in-game reset's button mask (spec/oxos_internal/22 §3); 0: off
    uint16_t PowerOffButtons; // the power-off combination's mask; 0: off
};

namespace oxos::ini_file {

// [dashboard]'s dashboard_<Number> (1 to INI_MAX_DASHBOARDS) as a device path in Path (its drive letter as the launch redirect file's, spec/oxos_internal/33
// §1; quotes around it are allowed); false when the file has none, or its path names no drive OXOS knows. A key given twice: the last one counts.
bool FindDashboard(const char *Text, uint32_t Length, uint32_t Number, char (&Path)[INI_REDIRECT_MAX_PATH + 1]);

// Reads [boot_video] from the file's text into *Settings: off and empty unless the file says otherwise ("yes" in any case turns a switch on; a key given twice,
// the last one counts). Errors counts the lines that could not be read, in the whole file.
void ReadBootVideoSettings(const char *Text, uint32_t Length, BOOT_VIDEO_SETTINGS *Settings, uint32_t *Errors);

// Reads [console] and [in_game_reset] from the file's text into *Settings, which starts at the defaults (solid green, the fan unchanged, LT RT BACK BLACK, no
// power-off combination). A value that cannot be used keeps its default and counts in Errors, as does a power-off combination that overlaps the reset's (it is
// then off); Errors also counts the lines that could not be read, in the whole file. A key given twice: the last one counts.
void ReadConsoleSettings(const char *Text, uint32_t Length, CONSOLE_SETTINGS *Settings, uint32_t *Errors);

} // namespace oxos::ini_file
