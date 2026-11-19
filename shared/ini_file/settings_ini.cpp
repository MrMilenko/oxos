// C:\OXOS\oxos_settings.ini, the kernel's own options: the parser (shared/ini_file/settings_ini.h).
#include <ini_file/settings_ini.h>
#include <ini_file/ini_file.h>
#include <c_library.h>

namespace oxos::ini_file {

namespace {

constexpr char DASHBOARD_SECTION[] = "dashboard";
constexpr char DASHBOARD_KEY[] = "dashboard_"; // followed by its number, 1 to INI_MAX_DASHBOARDS

bool IsSection(const INI_LINE &Line, const char *Name) {
    return Line.TextLength == strlen(Name) && strncasecmp(Line.Text, Name, Line.TextLength) == 0;
}

// A value without the quotes around it.
void Unquote(const char *&Value, uint32_t &Length) {
    if (Length >= 2 && Value[0] == '"' && Value[Length - 1] == '"') {
        Value++;
        Length -= 2;
    }
}

} // namespace

bool FindDashboard(const char *Text, uint32_t Length, uint32_t Number, char (&Path)[INI_REDIRECT_MAX_PATH + 1]) {
    const uint32_t prefix = sizeof(DASHBOARD_KEY) - 1;
    if (Number < 1 || Number > INI_MAX_DASHBOARDS)
        return false;
    bool found = false;
    bool inSection = false;
    INI_READER reader = {Text, Text + Length};
    INI_LINE line;
    while (NextLine(&reader, &line)) {
        if (line.Kind == INI_LINE_KIND::SECTION || line.Kind == INI_LINE_KIND::BAD_SECTION) {
            inSection = line.Kind == INI_LINE_KIND::SECTION && IsSection(line, DASHBOARD_SECTION);
            continue;
        }
        if (!inSection || line.Kind != INI_LINE_KIND::KEY || line.TextLength != prefix + 1 || strncasecmp(line.Text, DASHBOARD_KEY, prefix) != 0 ||
            line.Text[prefix] != static_cast<char>('0' + Number))
            continue;
        const char *value = line.Value;
        uint32_t valueLength = line.ValueLength;
        Unquote(value, valueLength);
        found = valueLength && RedirectDevicePath(value, valueLength, Path);
    }
    return found;
}

namespace {

constexpr char BOOT_VIDEO_SECTION[] = "boot_video";
constexpr char KEY_ENABLED[] = "boot_video_enabled";
constexpr char KEY_ALWAYS[] = "boot_video_always";
constexpr char KEY_FILE[] = "boot_video_file";
constexpr char YES[] = "yes";

bool IsYes(const INI_LINE &Line) {
    return Line.ValueLength == 3 && strncasecmp(Line.Value, YES, 3) == 0;
}

// boot_video_file: the value without the quotes around it; empty when it is too long.
void CopyFileName(const INI_LINE &Line, char (&File)[BOOT_VIDEO_MAX_FILE + 1]) {
    const char *from = Line.Value;
    uint32_t length = Line.ValueLength;
    Unquote(from, length);
    File[0] = 0;
    if (length <= BOOT_VIDEO_MAX_FILE)
        strlcpy(File, from, length + 1);
}

} // namespace

void ReadBootVideoSettings(const char *Text, uint32_t Length, BOOT_VIDEO_SETTINGS *Settings, uint32_t *Errors) {
    memset(Settings, 0, sizeof(*Settings));
    *Errors = 0;
    bool inSection = false;
    INI_READER reader = {Text, Text + Length};
    INI_LINE line;
    while (NextLine(&reader, &line)) {
        if (line.Kind == INI_LINE_KIND::SECTION || line.Kind == INI_LINE_KIND::BAD_SECTION) {
            inSection = line.Kind == INI_LINE_KIND::SECTION && IsSection(line, BOOT_VIDEO_SECTION);
            if (line.Kind == INI_LINE_KIND::BAD_SECTION)
                (*Errors)++;
            continue;
        }
        if (line.Kind == INI_LINE_KIND::BAD_LINE) {
            (*Errors)++;
            continue;
        }
        if (!inSection)
            continue;
        if (IsKey(line, KEY_ENABLED))
            Settings->Enabled = IsYes(line);
        else if (IsKey(line, KEY_ALWAYS))
            Settings->Always = IsYes(line);
        else if (IsKey(line, KEY_FILE))
            CopyFileName(line, Settings->File);
    }
}

// --- [console] and [in_game_reset] (spec/oxos_internal/44)

namespace {

constexpr char CONSOLE_SECTION[] = "console";
constexpr char IN_GAME_RESET_SECTION[] = "in_game_reset";
constexpr char KEY_FRONT_LED[] = "front_led";
constexpr char KEY_FAN_SPEED[] = "fan_speed";
constexpr char KEY_RESET_BUTTONS[] = "reset_buttons";
constexpr char KEY_POWER_OFF_BUTTONS[] = "power_off_buttons";
constexpr char FAN_AUTOMATIC[] = "auto";
constexpr char NO_BUTTONS[] = "none";

// The button names, in the order of their bits in the mask (spec/oxos_internal/22 §3): the digital buttons, then the analog ones.
constexpr const char *BUTTON_NAMES[16] = {"up", "down", "left", "right", "start", "back", "ls", "rs", "a", "b", "x", "y", "black", "white", "lt", "rt"};

bool IsValue(const INI_LINE &Line, const char *Word) {
    return Line.ValueLength == strlen(Word) && strncasecmp(Line.Value, Word, Line.ValueLength) == 0;
}

// front_led: 4 steps, each G (green), R (red), A (amber: both) or - (off), as the SMC's pattern; false for anything else.
bool ParseFrontLed(const INI_LINE &Line, uint8_t *Pattern) {
    if (Line.ValueLength != 4)
        return false;
    uint8_t pattern = 0;
    for (uint32_t i = 0; i < 4; i++) {
        const char c = Line.Value[i] | 0x20; // lower case; '-' is unchanged
        if (c == 'g' || c == 'a')
            pattern |= 1u << i;
        if (c == 'r' || c == 'a')
            pattern |= 0x10u << i;
        if (c != 'g' && c != 'r' && c != 'a' && c != '-')
            return false;
    }
    *Pattern = pattern;
    return true;
}

// fan_speed: auto, or CONSOLE_FAN_SLOWEST to 100; false for anything else.
bool ParseFanSpeed(const INI_LINE &Line, int8_t *Speed) {
    if (IsValue(Line, FAN_AUTOMATIC)) {
        *Speed = CONSOLE_FAN_AUTOMATIC;
        return true;
    }
    if (Line.ValueLength == 0 || Line.ValueLength > 3)
        return false;
    uint32_t speed = 0;
    for (uint32_t i = 0; i < Line.ValueLength; i++) {
        if (Line.Value[i] < '0' || Line.Value[i] > '9')
            return false;
        speed = speed * 10 + (Line.Value[i] - '0');
    }
    if (speed < static_cast<uint32_t>(CONSOLE_FAN_SLOWEST) || speed > 100)
        return false;
    *Speed = static_cast<int8_t>(speed);
    return true;
}

// A button combination: none (0), or at least CONSOLE_MIN_COMBINATION_BUTTONS names separated by blanks or '+'; false for an unknown name or too few.
bool ParseButtons(const INI_LINE &Line, uint16_t *Mask) {
    if (IsValue(Line, NO_BUTTONS)) {
        *Mask = 0;
        return true;
    }
    uint16_t mask = 0;
    uint32_t count = 0;
    const char *at = Line.Value;
    const char *end = Line.Value + Line.ValueLength;
    while (at < end) {
        if (*at == ' ' || *at == '\t' || *at == '+') {
            at++;
            continue;
        }
        const char *word = at;
        while (at < end && *at != ' ' && *at != '\t' && *at != '+')
            at++;
        const uint32_t length = static_cast<uint32_t>(at - word);
        uint32_t bit = 0;
        while (bit < 16 && !(strlen(BUTTON_NAMES[bit]) == length && strncasecmp(word, BUTTON_NAMES[bit], length) == 0))
            bit++;
        if (bit == 16)
            return false;
        if (!(mask & (1u << bit)))
            count++;
        mask |= static_cast<uint16_t>(1u << bit);
    }
    if (count < CONSOLE_MIN_COMBINATION_BUTTONS)
        return false;
    *Mask = mask;
    return true;
}

} // namespace

void ReadConsoleSettings(const char *Text, uint32_t Length, CONSOLE_SETTINGS *Settings, uint32_t *Errors) {
    Settings->FrontLed = CONSOLE_LED_SOLID_GREEN;
    Settings->FanSpeed = CONSOLE_FAN_UNCHANGED;
    Settings->ResetButtons = CONSOLE_RESET_BUTTONS;
    Settings->PowerOffButtons = 0;
    *Errors = 0;
    enum class SECTION { OTHER, CONSOLE, IN_GAME_RESET } section = SECTION::OTHER;
    INI_READER reader = {Text, Text + Length};
    INI_LINE line;
    while (NextLine(&reader, &line)) {
        if (line.Kind == INI_LINE_KIND::SECTION || line.Kind == INI_LINE_KIND::BAD_SECTION) {
            section = SECTION::OTHER;
            if (line.Kind == INI_LINE_KIND::BAD_SECTION)
                (*Errors)++;
            else if (IsSection(line, CONSOLE_SECTION))
                section = SECTION::CONSOLE;
            else if (IsSection(line, IN_GAME_RESET_SECTION))
                section = SECTION::IN_GAME_RESET;
            continue;
        }
        if (line.Kind == INI_LINE_KIND::BAD_LINE) {
            (*Errors)++;
            continue;
        }
        bool understood = true;
        if (section == SECTION::CONSOLE && IsKey(line, KEY_FRONT_LED))
            understood = ParseFrontLed(line, &Settings->FrontLed);
        else if (section == SECTION::CONSOLE && IsKey(line, KEY_FAN_SPEED))
            understood = ParseFanSpeed(line, &Settings->FanSpeed);
        else if (section == SECTION::IN_GAME_RESET && IsKey(line, KEY_RESET_BUTTONS))
            understood = ParseButtons(line, &Settings->ResetButtons);
        else if (section == SECTION::IN_GAME_RESET && IsKey(line, KEY_POWER_OFF_BUTTONS))
            understood = ParseButtons(line, &Settings->PowerOffButtons);
        if (!understood)
            (*Errors)++;
    }
    // Each combination needs a button the other does not have: a player holding one passes through the other on the way.
    const uint16_t reset = Settings->ResetButtons;
    const uint16_t powerOff = Settings->PowerOffButtons;
    if (reset && powerOff && ((reset & powerOff) == reset || (reset & powerOff) == powerOff)) {
        Settings->PowerOffButtons = 0;
        (*Errors)++;
    }
}

} // namespace oxos::ini_file
