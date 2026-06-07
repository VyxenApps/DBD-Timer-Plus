# DBD Timer Plus+ v0.3

Timer overlay for Dead by Daylight with dual timer support, perk builds, selection roulette, and notifications.

## Features

- **Dual timer** - Two independent countdown and count-up timers
- **Keyboard and mouse controls** - Configurable hotkeys for timer selection
- **Gamepad** - Full XInput support (Xbox / PlayStation)
- **Perk builds** - Build editor for Survivors and Killers with 500+ perks
- **Jason Voorhees perks** - 3 exclusive perks (Rampage, ScaredToDeath, SilentShadow)
- **Roulette** - Spinner wheel for random preset selection
- **Transparent overlay** - Always-on-top window with configurable transparency
- **Click-through** - Option to ignore clicks on the overlay
- **Color palette** - 105 customizable colors for timers and elements
- **Streak tracker** - Win/loss streak for Survivors and Killers
- **Notifications** - Toast overlay with toxic chat support

## Requirements

- Windows 10/11 (x64)
- Visual Studio 2026 or higher (to compile)
- DirectX Runtime (included in modern Windows)

### Dependencies

| Library | Usage |
|---------|-------|
| Direct2D | Overlay rendering |
| DirectWrite | Fonts and text |
| WIC | Image loading |
| GDI+ | Settings UI rendering |
| XInput | Gamepad support |
| JsonCpp | Configuration persistence |

## Configuration

Settings are saved in `AppPrefs.json` next to the executable.

## Updates
Currently, the application runs smoothly and can be used normally. Updates and improvements will be added gradually to ensure smooth performance.

