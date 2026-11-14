// mc3ds_cheat_engine.cpp
// Cheat Engine for Minecraft: New Nintendo 3DS Edition (v1.9.19)
// Target: Luma3DS 3GX Plugin (CTRPluginFramework + devkitARM)
//
// Verified memory offsets sourced from:
//   https://github.com/Minecraft-3DS-Community/GamePatches
// Note: code.bin offsets map to runtime RAM at (offset + 0x100000).

#include <CTRPluginFramework.hpp>
#include <3ds.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace CTRPluginFramework {

// ============================================================================
// 1. VERIFIED MEMORY ADDRESSES (Minecraft: New Nintendo 3DS Edition)
// ============================================================================
namespace Addr {
    constexpr u32 CODE_BASE_OFFSET = 0x00100000;

    // code.bin offsets (+ 0x100000 for runtime RAM)
    constexpr u32 PLAYER_REACH          = 0x54FB68 + CODE_BASE_OFFSET; // float, default 5.623
    constexpr u32 CAMERA_FOV            = 0x2CEE80 + CODE_BASE_OFFSET; // float, default 70.0
    constexpr u32 DAMAGE_MULTIPLIER     = 0x0C3E4C + CODE_BASE_OFFSET; // float, default 1.0
    constexpr u32 FALL_DAMAGE_CONST     = 0x4EBA90 + CODE_BASE_OFFSET; // float, default -9.01
    constexpr u32 JUMP_VELOCITY         = 0x6139F8 + CODE_BASE_OFFSET; // float, default 0.421
    constexpr u32 WORLD_GRAVITY         = 0x3ED468 + CODE_BASE_OFFSET; // float, default 0.08
    constexpr u32 PLAYER_RUN_SPEED      = 0x24BF14 + CODE_BASE_OFFSET; // float, default 1.0
    constexpr u32 SWIM_SPEED            = 0x3EA090 + CODE_BASE_OFFSET; // float, default 0.02
    constexpr u32 CREATIVE_FLY_SPEED    = 0x098D00 + CODE_BASE_OFFSET; // float, default 0.15
    constexpr u32 PLAYER_V_VELOCITY     = 0x09870C + CODE_BASE_OFFSET; // float, default 1.0
    constexpr u32 AIR_MOVEMENT_SPEED    = 0x502C64 + CODE_BASE_OFFSET; // float, default 0.02
    constexpr u32 CLOUD_HEIGHT          = 0x2C5398 + CODE_BASE_OFFSET; // float, default 0.33
    constexpr u32 CAMERA_SENSITIVITY    = 0x00B4D4 + CODE_BASE_OFFSET; // float, default 3.0
    constexpr u32 VIEW_BOBBING          = 0x2CF2A0 + CODE_BASE_OFFSET; // float, default 5.0
    constexpr u32 HOSTILE_MOB_COUNT     = 0x9338AC + CODE_BASE_OFFSET; // u32
    constexpr u32 LAND_ANIMAL_COUNT     = 0x9338A8 + CODE_BASE_OFFSET; // u32

    // Static RAM addresses (already mapped, no +0x100000 offset)
    constexpr u32 PLAYER_X              = 0x00AC1E48; // float (read-only)
    constexpr u32 PLAYER_Y              = 0x00AC1E4C; // float (read-only)
    constexpr u32 PLAYER_Z              = 0x00AC1E50; // float (read-only)
    constexpr u32 ENGINE_3D_RUNNING     = 0x3018AEF0; // u32 (1 = running)
}

// ============================================================================
// 2. NATIVE SETTINGS MENU HOOK CONFIGURATION (Ghidra Symbols Required)
// ============================================================================
// Set to 1 once you fill in the 3 code.bin addresses from your region's
// code.bin in Ghidra (USA: 000400000017FD00 / EUR: 00040000001B8700).
#define ENABLE_NATIVE_SETTINGS_HOOK 0

namespace SettingsHookAddr {
    // Replace 0x00000000 with (Ghidra_Offset + 0x100000) for your code.bin:
    // 1. OptionsScreen::render(OptionsScreen *this)
    constexpr u32 OPTIONS_SCREEN_RENDER = 0x00000000;
    // 2. OptionsScreen::handleInput(OptionsScreen *this, ...)
    constexpr u32 OPTIONS_SCREEN_INPUT  = 0x00000000;
    // 3. Optional: Active screen/menu state pointer or boolean in BSS
    constexpr u32 IS_IN_SETTINGS_MENU   = 0x00000000;
}

// ============================================================================
// 3. CHEAT ENGINE STATE & SAFE MEMORY HELPERS
// ============================================================================
struct CheatConfig {
    // Toggles
    bool godModeNoFall      = false;
    bool extendedReach      = false;
    bool speedBoost         = false;
    bool superJump          = false;
    bool lowGravity         = false;
    bool fastCreativeFly    = false;
    bool fastSwim           = false;
    bool oneHitDamage       = false;
    bool quakeProFov        = false;
    bool freezeHostileMobs  = false;

    // Tunable Values
    float reachValue        = 10.0f;
    float speedMultiplier   = 2.0f;
    float jumpValue         = 0.75f;
    float gravityValue      = 0.03f;
    float damageMultiplier  = 100.0f;
    float fovValue          = 100.0f;
    float flySpeedValue     = 0.45f;
    float swimSpeedValue    = 0.08f;

    // Native Settings Menu UI State
    bool settingsOverlayOpen = false;
    int  selectedRow         = 0;
};

static CheatConfig g_cfg;
static RT_HOOK     g_renderHook;
static RT_HOOK     g_inputHook;

static inline void SafeWriteFloat(u32 addr, float val) {
    if (Process::CheckAddress(addr)) {
        Process::WriteFloat(addr, val);
    }
}

static inline void SafeWrite32(u32 addr, u32 val) {
    if (Process::CheckAddress(addr)) {
        Process::Write32(addr, val);
    }
}

static inline float SafeReadFloat(u32 addr, float fallback = 0.0f) {
    float out = fallback;
    if (Process::CheckAddress(addr)) {
        Process::ReadFloat(addr, out);
    }
    return out;
}

static inline u32 SafeRead32(u32 addr, u32 fallback = 0) {
    u32 out = fallback;
    if (Process::CheckAddress(addr)) {
        Process::Read32(addr, out);
    }
    return out;
}

// Apply or restore all active cheats every frame
static void ApplyCheatsTick() {
    // 1. No Fall Damage (zeroes the Y-velocity fall damage constant)
    SafeWriteFloat(Addr::FALL_DAMAGE_CONST, g_cfg.godModeNoFall ? 0.0f : -9.01f);

    // 2. Extended Reach (default 5.623)
    SafeWriteFloat(Addr::PLAYER_REACH, g_cfg.extendedReach ? g_cfg.reachValue : 5.623f);

    // 3. Running Speed (default 1.0)
    SafeWriteFloat(Addr::PLAYER_RUN_SPEED, g_cfg.speedBoost ? g_cfg.speedMultiplier : 1.0f);

    // 4. Jump Velocity (default 0.421)
    SafeWriteFloat(Addr::JUMP_VELOCITY, g_cfg.superJump ? g_cfg.jumpValue : 0.421f);

    // 5. World Gravity (default 0.08)
    SafeWriteFloat(Addr::WORLD_GRAVITY, g_cfg.lowGravity ? g_cfg.gravityValue : 0.08f);

    // 6. Creative Mode Fly Speed (default 0.15)
    SafeWriteFloat(Addr::CREATIVE_FLY_SPEED, g_cfg.fastCreativeFly ? g_cfg.flySpeedValue : 0.15f);

    // 7. Swim Speed (default 0.02)
    SafeWriteFloat(Addr::SWIM_SPEED, g_cfg.fastSwim ? g_cfg.swimSpeedValue : 0.02f);

    // 8. Damage Multiplier (default 1.0)
    SafeWriteFloat(Addr::DAMAGE_MULTIPLIER, g_cfg.oneHitDamage ? g_cfg.damageMultiplier : 1.0f);

    // 9. Field of View (default 70.0)
    SafeWriteFloat(Addr::CAMERA_FOV, g_cfg.quakeProFov ? g_cfg.fovValue : 70.0f);

    // 10. Freeze / Clear Hostile Mob Count
    if (g_cfg.freezeHostileMobs) {
        SafeWrite32(Addr::HOSTILE_MOB_COUNT, 0);
    }
}

// ============================================================================
// 4. IN-SETTINGS-MENU CHEAT PANEL (Bottom Screen / Native Hook Renderer)
// ============================================================================
struct CheatOptionItem {
    const char *label;
    bool       *togglePtr;
    float      *floatPtr;
    float       step;
    float       minVal;
    float       maxVal;
};

static std::vector<CheatOptionItem> GetCheatItems() {
    return {
        {"No Fall Damage",        &g_cfg.godModeNoFall,     nullptr,                  0.0f,  0.0f,   0.0f},
        {"Extended Reach",        &g_cfg.extendedReach,     &g_cfg.reachValue,        1.0f,  5.0f,  25.0f},
        {"Speed Boost",           &g_cfg.speedBoost,        &g_cfg.speedMultiplier,   0.5f,  1.0f,   8.0f},
        {"Super Jump",            &g_cfg.superJump,         &g_cfg.jumpValue,         0.1f,  0.4f,   2.5f},
        {"Low Gravity",           &g_cfg.lowGravity,        &g_cfg.gravityValue,      0.01f, 0.01f,  0.20f},
        {"Fast Creative Fly",     &g_cfg.fastCreativeFly,   &g_cfg.flySpeedValue,     0.05f, 0.15f,  1.50f},
        {"Fast Swim",             &g_cfg.fastSwim,          &g_cfg.swimSpeedValue,    0.02f, 0.02f,  0.50f},
        {"Damage Multiplier",     &g_cfg.oneHitDamage,      &g_cfg.damageMultiplier, 10.0f,  1.0f, 500.0f},
        {"Custom FOV",            &g_cfg.quakeProFov,       &g_cfg.fovValue,          5.0f, 50.0f, 120.0f},
        {"Clear Hostile Mobs",    &g_cfg.freezeHostileMobs, nullptr,                  0.0f,  0.0f,   0.0f},
    };
}

// Draws the Cheat Engine panel directly onto the 3DS screen when inside Settings
static void DrawSettingsCheatPanel() {
    const Screen &scr = OSD::GetBottomScreen();
    auto items = GetCheatItems();

    // Panel background (320x240 bottom screen)
    scr.DrawRect(12, 10, 296, 220, Color(24, 24, 28, 235), true);
    scr.DrawRect(12, 10, 296, 220, Color(85, 255, 85), false);
    scr.Draw("MC3DS Cheat Engine (Settings)", 22, 18, Color(85, 255, 85));

    // Live player coordinates header
    float px = SafeReadFloat(Addr::PLAYER_X);
    float py = SafeReadFloat(Addr::PLAYER_Y);
    float pz = SafeReadFloat(Addr::PLAYER_Z);
    char coordBuf[64];
    std::snprintf(coordBuf, sizeof(coordBuf), "XYZ: %.1f, %.1f, %.1f", px, py, pz);
    scr.Draw(coordBuf, 22, 34, Color(180, 180, 180));

    int y = 54;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        bool selected = (i == g_cfg.selectedRow);
        Color rowCol  = selected ? Color(255, 255, 100) : Color::White;

        char line[64];
        if (items[i].floatPtr != nullptr) {
            std::snprintf(line, sizeof(line), "%s %-18s [%s] (%.2f)",
                selected ? ">" : " ",
                items[i].label,
                *(items[i].togglePtr) ? "ON" : "OFF",
                *(items[i].floatPtr));
        } else {
            std::snprintf(line, sizeof(line), "%s %-18s [%s]",
                selected ? ">" : " ",
                items[i].label,
                *(items[i].togglePtr) ? "ON" : "OFF");
        }
        scr.Draw(line, 20, y, rowCol);
        y += 15;
    }

    scr.Draw("D-Pad: Nav/Adjust | A: Toggle | B: Close", 20, 212, Color(160, 220, 255));
}

static void HandleSettingsCheatInput() {
    auto items = GetCheatItems();
    int count = static_cast<int>(items.size());

    if (Controller::IsKeyPressed(Key::DPadDown)) {
        g_cfg.selectedRow = (g_cfg.selectedRow + 1) % count;
    } else if (Controller::IsKeyPressed(Key::DPadUp)) {
        g_cfg.selectedRow = (g_cfg.selectedRow - 1 + count) % count;
    } else if (Controller::IsKeyPressed(Key::A)) {
        *(items[g_cfg.selectedRow].togglePtr) = !(*(items[g_cfg.selectedRow].togglePtr));
    } else if (Controller::IsKeyPressed(Key::DPadRight) && items[g_cfg.selectedRow].floatPtr) {
        float *v = items[g_cfg.selectedRow].floatPtr;
        *v += items[g_cfg.selectedRow].step;
        if (*v > items[g_cfg.selectedRow].maxVal) *v = items[g_cfg.selectedRow].maxVal;
    } else if (Controller::IsKeyPressed(Key::DPadLeft) && items[g_cfg.selectedRow].floatPtr) {
        float *v = items[g_cfg.selectedRow].floatPtr;
        *v -= items[g_cfg.selectedRow].step;
        if (*v < items[g_cfg.selectedRow].minVal) *v = items[g_cfg.selectedRow].minVal;
    } else if (Controller::IsKeyPressed(Key::B)) {
        g_cfg.settingsOverlayOpen = false;
    }
}

#if ENABLE_NATIVE_SETTINGS_HOOK
// Hooked into OptionsScreen::render
static void Hook_OptionsScreenRender(void *screenPtr) {
    // Call original Minecraft 3DS OptionsScreen::render
    ((void (*)(void *))g_renderHook.callCode)(screenPtr);

    // Draw our Cheat Engine tab/panel inside the Settings screen
    if (g_cfg.settingsOverlayOpen) {
        DrawSettingsCheatPanel();
    } else {
        const Screen &scr = OSD::GetBottomScreen();
        scr.Draw("[L+R] Open Cheat Engine", 160, 224, Color(85, 255, 85));
    }
}

// Hooked into OptionsScreen::handleInput
static void Hook_OptionsScreenInput(void *screenPtr, void *inputData) {
    if (Controller::IsKeysDown(Key::L | Key::R)) {
        g_cfg.settingsOverlayOpen = !g_cfg.settingsOverlayOpen;
    }

    if (g_cfg.settingsOverlayOpen) {
        HandleSettingsCheatInput();
        return; // Consume input while Cheat Engine panel is open
    }

    ((void (*)(void *, void *))g_inputHook.callCode)(screenPtr, inputData);
}
#endif

// ============================================================================
// 5. CTRPF MENU ENTRIES (Always Available via SELECT + Settings Quick-Toggle)
// ============================================================================
static void Entry_ToggleOverlay(MenuEntry *entry) {
    g_cfg.settingsOverlayOpen = !g_cfg.settingsOverlayOpen;
    OSD::Notify(g_cfg.settingsOverlayOpen ? "Cheat Engine Panel: OPEN" : "Cheat Engine Panel: CLOSED");
}

static void Entry_ToggleBool(MenuEntry *entry) {
    bool *flag = static_cast<bool *>(entry->GetArg());
    if (flag) {
        *flag = entry->IsActivated();
    }
}

static void Entry_SetFloat(MenuEntry *entry) {
    float *target = static_cast<float *>(entry->GetArg());
    if (!target) return;
    Keyboard kb("Enter new value:");
    kb.Open(*target, *target);
}

static void Entry_ShowPlayerStats(MenuEntry *entry) {
    float px = SafeReadFloat(Addr::PLAYER_X);
    float py = SafeReadFloat(Addr::PLAYER_Y);
    float pz = SafeReadFloat(Addr::PLAYER_Z);
    u32 mobs = SafeRead32(Addr::HOSTILE_MOB_COUNT);
    u32 animals = SafeRead32(Addr::LAND_ANIMAL_COUNT);

    std::string msg = Utils::Format(
        "Player Position:\n  X: %.2f\n  Y: %.2f\n  Z: %.2f\n\nWorld Entities:\n  Hostile Mobs: %u\n  Land Animals: %u",
        px, py, pz, mobs, animals
    );
    MessageBox("MC3DS Live Memory", msg)();
}

// Background callback executed every frame
static void MainLoopCallback() {
    ApplyCheatsTick();

    // Fallback hotkey (L + R + DPad-Down) to open the in-game Settings Cheat Panel
    // even before Ghidra native OptionsScreen hooks are enabled.
    if (Controller::IsKeysPressed(Key::L | Key::R | Key::DPadDown)) {
        g_cfg.settingsOverlayOpen = !g_cfg.settingsOverlayOpen;
    }

#if !ENABLE_NATIVE_SETTINGS_HOOK
    if (g_cfg.settingsOverlayOpen) {
        HandleSettingsCheatInput();
        DrawSettingsCheatPanel();
    }
#endif
}

static MenuFolder *BuildCheatMenu() {
    auto *root = new MenuFolder("MC3DS Cheat Engine");

    root->Append(new MenuEntry("Open In-Screen Cheat Panel (L+R+Down)", nullptr, Entry_ToggleOverlay));
    root->Append(new MenuEntry("View Live Coordinates & Mobs", nullptr, Entry_ShowPlayerStats));

    auto *movement = new MenuFolder("Movement & Physics");
    auto *eSpeed   = new MenuEntry("Speed Boost", Entry_ToggleBool);
    eSpeed->SetArg(&g_cfg.speedBoost);
    movement->Append(eSpeed);

    auto *eJump    = new MenuEntry("Super Jump", Entry_ToggleBool);
    eJump->SetArg(&g_cfg.superJump);
    movement->Append(eJump);

    auto *eGrav    = new MenuEntry("Low Gravity", Entry_ToggleBool);
    eGrav->SetArg(&g_cfg.lowGravity);
    movement->Append(eGrav);

    auto *eFly     = new MenuEntry("Fast Creative Fly", Entry_ToggleBool);
    eFly->SetArg(&g_cfg.fastCreativeFly);
    movement->Append(eFly);

    auto *eSwim    = new MenuEntry("Fast Swim", Entry_ToggleBool);
    eSwim->SetArg(&g_cfg.fastSwim);
    movement->Append(eSwim);
    root->Append(movement);

    auto *combat   = new MenuFolder("Combat & World");
    auto *eNoFall  = new MenuEntry("No Fall Damage", Entry_ToggleBool);
    eNoFall->SetArg(&g_cfg.godModeNoFall);
    combat->Append(eNoFall);

    auto *eReach   = new MenuEntry("Extended Reach", Entry_ToggleBool);
    eReach->SetArg(&g_cfg.extendedReach);
    combat->Append(eReach);

    auto *eDmg     = new MenuEntry("High Damage Multiplier", Entry_ToggleBool);
    eDmg->SetArg(&g_cfg.oneHitDamage);
    combat->Append(eDmg);

    auto *eFov     = new MenuEntry("Custom Camera FOV", Entry_ToggleBool);
    eFov->SetArg(&g_cfg.quakeProFov);
    combat->Append(eFov);

    auto *eMobs    = new MenuEntry("Clear Hostile Mobs", Entry_ToggleBool);
    eMobs->SetArg(&g_cfg.freezeHostileMobs);
    combat->Append(eMobs);
    root->Append(combat);

    auto *tuning   = new MenuFolder("Value Tuning");
    auto *tReach   = new MenuEntry("Set Reach Distance", nullptr, Entry_SetFloat);
    tReach->SetArg(&g_cfg.reachValue);
    tuning->Append(tReach);

    auto *tSpeed   = new MenuEntry("Set Speed Multiplier", nullptr, Entry_SetFloat);
    tSpeed->SetArg(&g_cfg.speedMultiplier);
    tuning->Append(tSpeed);

    auto *tJump    = new MenuEntry("Set Jump Velocity", nullptr, Entry_SetFloat);
    tJump->SetArg(&g_cfg.jumpValue);
    tuning->Append(tJump);

    auto *tFov     = new MenuEntry("Set Camera FOV", nullptr, Entry_SetFloat);
    tFov->SetArg(&g_cfg.fovValue);
    tuning->Append(tFov);
    root->Append(tuning);

    return root;
}

// ============================================================================
// 6. PLUGIN ENTRY POINT
// ============================================================================
int main() {
    auto *menu = new PluginMenu("MC3DS Cheat Engine", 1, 0, 0,
        "In-Settings Cheat Engine for Minecraft: New Nintendo 3DS Edition.");
    menu->SynchronizeWithFrame(true);

    menu->Append(BuildCheatMenu());
    menu->Callback(MainLoopCallback);

#if ENABLE_NATIVE_SETTINGS_HOOK
    if (SettingsHookAddr::OPTIONS_SCREEN_RENDER != 0) {
        rtInitHook(&g_renderHook, SettingsHookAddr::OPTIONS_SCREEN_RENDER,
                   reinterpret_cast<u32>(Hook_OptionsScreenRender));
        rtEnableHook(&g_renderHook);
    }
    if (SettingsHookAddr::OPTIONS_SCREEN_INPUT != 0) {
        rtInitHook(&g_inputHook, SettingsHookAddr::OPTIONS_SCREEN_INPUT,
                   reinterpret_cast<u32>(Hook_OptionsScreenInput));
        rtEnableHook(&g_inputHook);
    }
#endif

    menu->Run();
    delete menu;
    return 0;
}

} // namespace CTRPluginFramework