// oxos_settings.ini parser (spec/oxos_internal/38 §1, kernel/src/ini_file/settings_ini.cpp).
#include "kernel_test.h"

#include <cstring>
#include <string>

#include <ini_file/settings_ini.h>

using namespace xk; // the kernel's subsystems as storage::, titles::, ...

namespace {

struct Result {
    BOOT_VIDEO_SETTINGS Settings;
    uint32_t Errors;
};

Result Read(const std::string &text) {
    Result result;
    memset(&result.Settings, 0x55, sizeof(result.Settings));
    result.Errors = 99;
    oxos::ini_file::ReadBootVideoSettings(text.data(), (uint32_t)text.size(), &result.Settings, &result.Errors);
    return result;
}

} // namespace

TEST(SettingsIni, BootVideoAsTheSpecShowsIt) {
    Result r = Read("; C:\\OXOS\\oxos_settings.ini\r\n[boot_video]\r\n; on\r\nboot_video_enabled = yes\r\n"
                    "boot_video_file = intro.mpg\r\nboot_video_always = no\r\n");
    EXPECT_TRUE(r.Settings.Enabled);
    EXPECT_FALSE(r.Settings.Always);
    EXPECT_STREQ(r.Settings.File, "intro.mpg");
    EXPECT_EQ(r.Errors, 0u);
}

TEST(SettingsIni, NothingSaidIsOff) {
    Result r = Read("");
    EXPECT_FALSE(r.Settings.Enabled);
    EXPECT_FALSE(r.Settings.Always);
    EXPECT_STREQ(r.Settings.File, "");
    r = Read("[dashboard]\norder = C:\\xboxdash.xbe\n");
    EXPECT_FALSE(r.Settings.Enabled);
    EXPECT_EQ(r.Errors, 0u);
}

TEST(SettingsIni, OnlyYesTurnsASwitchOn) {
    EXPECT_TRUE(Read("[boot_video]\nboot_video_enabled = YES\n").Settings.Enabled);
    EXPECT_TRUE(Read("[Boot_Video]\nBOOT_VIDEO_ALWAYS=Yes\n").Settings.Always);
    EXPECT_FALSE(Read("[boot_video]\nboot_video_enabled = true\n").Settings.Enabled);
    EXPECT_FALSE(Read("[boot_video]\nboot_video_enabled = yes ; on\n").Settings.Enabled); // a ';' after a value is part of it
    EXPECT_FALSE(Read("[boot_video]\nboot_video_enabled =\n").Settings.Enabled);
}

TEST(SettingsIni, KeysOutsideTheSectionAreIgnored) {
    Result r = Read("boot_video_enabled = yes\n[other]\nboot_video_enabled = yes\n[boot_video]\nboot_video_file = a.mpg\n[after]\nboot_video_always = yes\n");
    EXPECT_FALSE(r.Settings.Enabled);
    EXPECT_FALSE(r.Settings.Always);
    EXPECT_STREQ(r.Settings.File, "a.mpg");
}

TEST(SettingsIni, TheFileLosesItsQuotes) {
    EXPECT_STREQ(Read("[boot_video]\nboot_video_file = \"my intro.mpg\"\n").Settings.File, "my intro.mpg");
    EXPECT_STREQ(Read("[boot_video]\nboot_video_file = E:\\Videos\\x.mpg\n").Settings.File, "E:\\Videos\\x.mpg");
    EXPECT_STREQ(Read("[boot_video]\nboot_video_file = \"half.mpg\n").Settings.File, "\"half.mpg");
    std::string longest(BOOT_VIDEO_MAX_FILE, 'a');
    EXPECT_STREQ(Read("[boot_video]\nboot_video_file = " + longest + "\n").Settings.File, longest.c_str());
    EXPECT_STREQ(Read("[boot_video]\nboot_video_file = " + longest + "a\n").Settings.File, "");
}

TEST(SettingsIni, TheLastOfTwoKeysCounts) {
    Result r = Read("[boot_video]\nboot_video_enabled = yes\nboot_video_enabled = no\n[boot_video]\nboot_video_always = no\nboot_video_always = yes\n");
    EXPECT_FALSE(r.Settings.Enabled);
    EXPECT_TRUE(r.Settings.Always);
}

TEST(SettingsIni, UnreadableLinesAreCounted) {
    Result r = Read("[boot_video\nnot a key\n[boot_video]\nboot_video_enabled = yes\n= value\n");
    EXPECT_TRUE(r.Settings.Enabled);
    EXPECT_EQ(r.Errors, 3u);
}

namespace {

std::string Dashboard(const std::string &text, uint32_t number) {
    char path[INI_REDIRECT_MAX_PATH + 1];
    return oxos::ini_file::FindDashboard(text.data(), (uint32_t)text.size(), number, path) ? path : "(none)";
}

} // namespace

TEST(SettingsIni, DashboardsAsTheShippedFileListsThem) {
    std::string text = "; C:\\OXOS\\oxos_settings.ini\r\n[dashboard]\r\n; the order\r\ndashboard_1 = C:\\evoxdash.xbe\r\ndashboard_2 = E:\\evoxdash.xbe\r\n"
                       "dashboard_3 = C:\\xboxdash.xbe\r\ndashboard_4 = C:\\Dashboard\\default.xbe\r\n[boot_video]\r\nboot_video_enabled = no\r\n";
    EXPECT_EQ(Dashboard(text, 1), "\\Device\\Harddisk0\\Partition2\\evoxdash.xbe");
    EXPECT_EQ(Dashboard(text, 2), "\\Device\\Harddisk0\\Partition1\\evoxdash.xbe");
    EXPECT_EQ(Dashboard(text, 3), "\\Device\\Harddisk0\\Partition2\\xboxdash.xbe");
    EXPECT_EQ(Dashboard(text, 4), "\\Device\\Harddisk0\\Partition2\\Dashboard\\default.xbe");
    EXPECT_EQ(Dashboard(text, 5), "(none)");
}

TEST(SettingsIni, DashboardNumbersOutsideTheListAreNone) {
    std::string text = "[dashboard]\ndashboard_0 = C:\\a.xbe\ndashboard_9 = C:\\b.xbe\ndashboard_10 = C:\\c.xbe\n";
    EXPECT_EQ(Dashboard(text, 0), "(none)");
    EXPECT_EQ(Dashboard(text, 9), "(none)");
    EXPECT_EQ(Dashboard(text, 1), "(none)");
}

TEST(SettingsIni, DashboardKeysOnlyInTheirSection) {
    std::string text = "dashboard_1 = C:\\top.xbe\n[boot_video]\ndashboard_1 = C:\\video.xbe\n[Dashboard]\nDASHBOARD_2 = \"F:\\Apps\\XBMC\\default.xbe\"\n";
    EXPECT_EQ(Dashboard(text, 1), "(none)");
    EXPECT_EQ(Dashboard(text, 2), "\\Device\\Harddisk0\\Partition6\\Apps\\XBMC\\default.xbe");
}

TEST(SettingsIni, DashboardPathsOnUnknownDrivesAreNone) {
    std::string text = "[dashboard]\ndashboard_1 = Q:\\x.xbe\ndashboard_2 =\ndashboard_3 = e:/evoxdash.xbe\ndashboard_3 = C:\\last.xbe\n";
    EXPECT_EQ(Dashboard(text, 1), "(none)");
    EXPECT_EQ(Dashboard(text, 2), "(none)");
    EXPECT_EQ(Dashboard(text, 3), "\\Device\\Harddisk0\\Partition2\\last.xbe");
}

// --- [console] and [in_game_reset] (spec/oxos_internal/44)

namespace {

struct ConsoleResult {
    CONSOLE_SETTINGS Settings;
    uint32_t Errors;
};

ConsoleResult ReadConsole(const std::string &text) {
    ConsoleResult result;
    memset(&result.Settings, 0x55, sizeof(result.Settings));
    result.Errors = 99;
    oxos::ini_file::ReadConsoleSettings(text.data(), (uint32_t)text.size(), &result.Settings, &result.Errors);
    return result;
}

} // namespace

TEST(SettingsIni, ConsoleDefaultsAreTodaysBehaviour) {
    for (const char *text : {"", "[console]\n[in_game_reset]\n", "[dashboard]\ndashboard_1 = C:\\xboxdash.xbe\n"}) {
        ConsoleResult r = ReadConsole(text);
        EXPECT_EQ(r.Settings.FrontLed, 0x0F);
        EXPECT_EQ(r.Settings.FanSpeed, CONSOLE_FAN_UNCHANGED);
        EXPECT_EQ(r.Settings.ResetButtons, 0xD020);
        EXPECT_EQ(r.Settings.PowerOffButtons, 0);
        EXPECT_EQ(r.Errors, 0u);
    }
}

TEST(SettingsIni, ConsoleAsTheSpecShowsIt) {
    ConsoleResult r = ReadConsole("[console]\r\nfront_led = GGGG\r\nfan_speed = auto\r\n[in_game_reset]\r\nreset_buttons = LT RT BACK BLACK\r\n"
                                  "power_off_buttons = none\r\n");
    EXPECT_EQ(r.Settings.FrontLed, 0x0F);
    EXPECT_EQ(r.Settings.FanSpeed, CONSOLE_FAN_AUTOMATIC);
    EXPECT_EQ(r.Settings.ResetButtons, 0xD020);
    EXPECT_EQ(r.Settings.PowerOffButtons, 0);
    EXPECT_EQ(r.Errors, 0u);
}

TEST(SettingsIni, FrontLedPatterns) {
    EXPECT_EQ(ReadConsole("[console]\nfront_led = RRRR\n").Settings.FrontLed, 0xF0);
    EXPECT_EQ(ReadConsole("[console]\nfront_led = aaaa\n").Settings.FrontLed, 0xFF);
    EXPECT_EQ(ReadConsole("[console]\nfront_led = GRGR\n").Settings.FrontLed, 0xA5);
    EXPECT_EQ(ReadConsole("[console]\nfront_led = G---\n").Settings.FrontLed, 0x01);
    EXPECT_EQ(ReadConsole("[console]\nfront_led = ----\n").Settings.FrontLed, 0x00);
    for (const char *bad : {"GGG", "GGGGG", "GGXG", "", "G G "}) {
        ConsoleResult r = ReadConsole(std::string("[console]\nfront_led = ") + bad + "\n");
        EXPECT_EQ(r.Settings.FrontLed, 0x0F) << bad;
        EXPECT_EQ(r.Errors, 1u) << bad;
    }
}

TEST(SettingsIni, FanSpeeds) {
    EXPECT_EQ(ReadConsole("[console]\nfan_speed = 20\n").Settings.FanSpeed, 20);
    EXPECT_EQ(ReadConsole("[console]\nfan_speed = 100\n").Settings.FanSpeed, 100);
    EXPECT_EQ(ReadConsole("[console]\nFAN_SPEED = AUTO\n").Settings.FanSpeed, CONSOLE_FAN_AUTOMATIC);
    for (const char *bad : {"19", "0", "101", "1000", "-5", "fast", "50%", ""}) {
        ConsoleResult r = ReadConsole(std::string("[console]\nfan_speed = ") + bad + "\n");
        EXPECT_EQ(r.Settings.FanSpeed, CONSOLE_FAN_UNCHANGED) << bad;
        EXPECT_EQ(r.Errors, 1u) << bad;
    }
}

TEST(SettingsIni, ResetButtonNames) {
    EXPECT_EQ(ReadConsole("[in_game_reset]\nreset_buttons = lt+rt+start+back\n").Settings.ResetButtons, 0xC030);
    EXPECT_EQ(ReadConsole("[in_game_reset]\nreset_buttons = UP DOWN LEFT RIGHT\n").Settings.ResetButtons, 0x000F);
    EXPECT_EQ(ReadConsole("[in_game_reset]\nreset_buttons = LS RS A B X Y WHITE\n").Settings.ResetButtons, 0x2FC0);
    EXPECT_EQ(ReadConsole("[in_game_reset]\nreset_buttons = none\n").Settings.ResetButtons, 0);
}

TEST(SettingsIni, ResetButtonsNeedThreeKnownButtons) {
    for (const char *bad : {"LT RT", "LT LT RT", "A", "LT RT BACK SELECT", "", "LT RT BACK BLACK ; reset"}) {
        ConsoleResult r = ReadConsole(std::string("[in_game_reset]\nreset_buttons = ") + bad + "\n");
        EXPECT_EQ(r.Settings.ResetButtons, 0xD020) << bad;
        EXPECT_EQ(r.Errors, 1u) << bad;
    }
}

TEST(SettingsIni, PowerOffMustNotOverlapTheReset) {
    ConsoleResult r = ReadConsole("[in_game_reset]\npower_off_buttons = LT RT START\n");
    EXPECT_EQ(r.Settings.PowerOffButtons, 0xC010);
    EXPECT_EQ(r.Errors, 0u);
    r = ReadConsole("[in_game_reset]\npower_off_buttons = LT RT BACK\n"); // inside LT RT BACK BLACK
    EXPECT_EQ(r.Settings.PowerOffButtons, 0);
    EXPECT_EQ(r.Errors, 1u);
    r = ReadConsole("[in_game_reset]\npower_off_buttons = LT RT BACK BLACK WHITE\n"); // holds all of the reset's
    EXPECT_EQ(r.Settings.PowerOffButtons, 0);
    EXPECT_EQ(r.Errors, 1u);
    r = ReadConsole("[in_game_reset]\nreset_buttons = none\npower_off_buttons = LT RT BACK\n"); // no reset: nothing to overlap
    EXPECT_EQ(r.Settings.PowerOffButtons, 0xC020);
    EXPECT_EQ(r.Errors, 0u);
}

TEST(SettingsIni, ConsoleKeysOnlyInTheirSections) {
    ConsoleResult r = ReadConsole("front_led = RRRR\n[in_game_reset]\nfan_speed = 50\n[console]\nreset_buttons = none\n[boot_video]\nfront_led = AAAA\n");
    EXPECT_EQ(r.Settings.FrontLed, 0x0F);
    EXPECT_EQ(r.Settings.FanSpeed, CONSOLE_FAN_UNCHANGED);
    EXPECT_EQ(r.Settings.ResetButtons, 0xD020);
    EXPECT_EQ(r.Errors, 0u);
}

TEST(SettingsIni, ConsoleTheLastOfTwoKeysCounts) {
    ConsoleResult r = ReadConsole("[console]\nfan_speed = 50\nfan_speed = auto\n[Console]\nfront_led = RRRR\n");
    EXPECT_EQ(r.Settings.FanSpeed, CONSOLE_FAN_AUTOMATIC);
    EXPECT_EQ(r.Settings.FrontLed, 0xF0);
}
