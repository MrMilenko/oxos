// In-game reset (src/in_game_reset/, spec/oxos_internal/22, 44 §4): LT + RT + BACK + BLACK (or oxos_settings.ini's reset_buttons) held in a title
// power-cycles the console; power_off_buttons turns it off. xinput_hook.S watches
// XInputGetState and calls HalpIgrCheck (a C name: assembly calls it).
#pragma once
#include <runtime/types.h>

XBAPI void NTAPI HalpIgrCheck(const UCHAR *State);

namespace xk::in_game_reset {

// A power cycle through the SMC, so the next boot is a cold one; the reference reset if the SMC does not cut the power within two seconds. Works at any IRQL
// (the SMBus is polled).
void Reboot();

// The button masks (spec/oxos_internal/22 §3) of the reset and the power-off combinations, 0 for off: the settings module's, at every launch
// (spec/oxos_internal/44 §4). Until then (and without the module): LT RT BACK BLACK, and no power-off.
void SetButtons(USHORT Reset, USHORT PowerOff);

} // namespace xk::in_game_reset
