// In-game reset (spec/oxos_internal/22, 44 §4). xinput_hook.S passes every controller state a title reads through XInputGetState; LT + RT + BACK + BLACK (or
// oxos_settings.ini's reset_buttons) held together power-cycles the console (a cold boot: a netbooted console fetches its image again and starts the launcher,
// a mounted disc image is dropped).
#include <in_game_reset/in_game_reset.h>
#include <hardware/chipset/chipset.h>
#include <diagnostics/diagnostics.h>
#include <runtime/types.h>
#include <core/core.h>

namespace xk::in_game_reset {

namespace {
// Cerbios's button mask (spec/cerbios/08 §2): bits 0-7 the digital buttons, bit 8 + i analog button i (A B X Y BLACK WHITE LT RT) held fully down (0xFF).
constexpr ULONG IGR_BACK = 0x0020, IGR_BLACK = 0x1000, IGR_LT = 0x4000, IGR_RT = 0x8000;

constexpr ULONG HAL_REBOOT_ROUTINE = 1;            // HalReturnToFirmware: a full reboot
constexpr UCHAR SMC_ADDRESS = 0x20, SMC_RESET = 0x02; // the SMC and its power register
constexpr UCHAR SMC_POWER_CYCLE = 0x40, SMC_POWER_OFF = 0x80;

bool InGameResetSeen;                                         // the first XInputGetState was logged
USHORT ResetButtons = IGR_LT | IGR_RT | IGR_BACK | IGR_BLACK; // the reset combination (0xD020, Cerbios's IGRFULL default); 0: off
USHORT PowerOffButtons;                                       // the power-off combination; 0: off

// Asks the SMC for Action on its power register, up to five tries, then waits two seconds for the power to go.
void RequestPower(UCHAR Action) {
    for (int i = 0; i < 5 && !NT_SUCCESS(HalWriteSMBusValue(SMC_ADDRESS, SMC_RESET, FALSE, Action)); i++) {
        // Retry until the SMC accepts the request (up to five tries).
    }
    for (ULONG ms = 0; ms < 2000; ms++)
        KeStallExecutionProcessor(1000);
}
} // namespace

// The in-game reset's reboot: a power cycle through the SMC (register 0x02 = 0x40, Cerbios's IGRCYCLE), so the next boot is a cold one. A reset (0x02 = 1, then
// port 0xCF9, HalReturnToFirmware 1) while a title is using the GPU left most of the X-codes' GPU and memory set-up undone at the next boot (29 settings at
// their power-on values, the RAM mis-sized and the kernel stopped, spec/oxos_internal/22 §5); port 0xCF9 alone does not reset the console at all. The SMBus is
// polled, so this works at any IRQL (XInputGetState may be called at DISPATCH_LEVEL). If the SMC does not cut the power within two seconds, the reference reset
// follows.
void Reboot() {
    RequestPower(SMC_POWER_CYCLE);
    HalReturnToFirmware(HAL_REBOOT_ROUTINE);
}

void SetButtons(USHORT Reset, USHORT PowerOff) {
    ResetButtons = Reset;
    PowerOffButtons = PowerOff;
}

} // namespace xk::in_game_reset

using namespace xk; // in_game_reset::

// State: XINPUT_STATE (packet number, then the gamepad: buttons u16 at +4, 8 analog bytes at +6).
XBAPI void NTAPI HalpIgrCheck(const UCHAR *State) {
    if (!in_game_reset::InGameResetSeen) {
        in_game_reset::InGameResetSeen = true;
        XK_SERIAL("igr: the title reads its controllers (XInputGetState)\n");
    }
    ULONG mask = State[4];
    for (ULONG i = 0; i < 8; i++) {
        if (State[6 + i] == 0xFF)
            mask |= 0x100u << i;
    }
    const ULONG powerOff = in_game_reset::PowerOffButtons;
    if (powerOff && (mask & powerOff) == powerOff) {
        XK_SERIAL("igr: %#lx: power off\n", powerOff);
        in_game_reset::RequestPower(in_game_reset::SMC_POWER_OFF);
        for (;;) {
            // Wait for the SMC to turn the power off.
        }
    }
    const ULONG reset = in_game_reset::ResetButtons;
    if (reset && (mask & reset) == reset) {
        XK_SERIAL("igr: %#lx: reboot\n", reset);
        in_game_reset::Reboot();
    }
}
