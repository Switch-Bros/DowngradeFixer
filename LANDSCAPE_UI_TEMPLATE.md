# Landscape UI Rotation Template for Nintendo Switch Homebrew

## Overview

This document describes the software-level landscape UI rotation logic implemented in Lockpick RCM Pro, designed for the Nintendo Switch's 720x1280 display. The landscape orientation is achieved through **hardware display controller flags** combined with **software rendering changes** to the framebuffer.

**Important:** The display hardware (TMOS-LMS** display controller) supports hardware rotation via `SCAN_COLUMN | H_DIRECTION` flags. This template focuses on the **software changes** needed for proper UI layout in landscape mode.

---

## 1. Hardware Display Configuration

### Display Controller Flags (bdk/display/di.inl)

The landscape mode is enabled at the hardware level with these flags:

```c
{DC_WIN_WIN_OPTIONS, WIN_ENABLE | SCAN_COLUMN | H_DIRECTION}
```

| Flag | Description | Effect |
|------|-------------|--------|
| `WIN_ENABLE` | Enable window | Activates the window output |
| `SCAN_COLUMN` | Scan column mode | Rotates display output 90° |
| `H_DIRECTION` | Horizontal direction | Sets landscape orientation |

**Source file:** `bdk/display/di.inl` (line ~718)

**Equivalent definitions in** `bdk/display/di.h`:
```c
#define H_DIRECTION     BIT(0)
#define SCAN_COLUMN     BIT(4)
```

---

## 2. Coordinate System Transformation

### Framebuffer vs Display Coordinates

The Switch display framebuffer is **physically portrait** (720×1280), but the hardware rotates it for **landscape output** (1280×720).

### Transformation Mapping

```
┌─────────────────────────────────────────────────────────────┐
│  Display Coordinates (1280 × 720)                            │
│  ┌────────────────────┬────────────────────┐               │
│  │                    │                    │               │
│  │   x = 0 → 1279    │   y = 0 → 719     │               │
│  │   (right)         │   (down)          │               │
│  └────────────────────┴────────────────────┘               │
└─────────────────────────────────────────────────────────────┘
         ↓ Rotation (90° clockwise)
         ↓
┌─────────────────────────────────────────────────────────────┐
│  Framebuffer Coordinates (720 × 1280)                       │
│  ┌────────────────────┬────────────────────┐               │
│  │                    │                    │               │
│  │   y = 0 → 1279    │   x = 0 → 719     │               │
│  │   (down)          │   (right)         │               │
│  └────────────────────┴────────────────────┘               │
└─────────────────────────────────────────────────────────────┘
```

### Conversion Formulas

| Operation | Formula |
|-----------|---------|
| Display X → Framebuffer Y | `fb_y = 1279 - display_x` |
| Display Y → Framebuffer X | `fb_x = display_y` |
| Pixel access | `fb[fb_y * stride + fb_x]` |

### Practical Example

```c
// Set cursor position for landscape display
// gfx_con_setpos(x, y) where:
//   x = horizontal position (0-1279) → internal y
//   y = vertical position (0-719)   → internal x

// Position at landscape (100, 50) becomes framebuffer (50, 1179)
gfx_con_setpos(50, 1179);  // Internal coordinates
// Appears at display position (x=50, y=100)
```

---

## 3. UI Layout Constants (gfx.h)

### Core Positioning Constants

Add these constants to your project's `gfx.h`:

```c
// ============================================================================
// Global UI Layout Settings - Landscape Mode
// ============================================================================
// Change these values to adjust menu/text positions across ALL screens
// ============================================================================

// Main Menu Position (tui.c)
// NOTE: 16px is automatically added by leading space in menu text
#define UI_MENU_START_X    5    // Base position (text actually starts at +16px)
#define UI_MENU_START_Y    32   // Vertical position of menu items (from top, below title bar)
#define UI_MENU_SPACING    24   // Vertical spacing between menu items

// Dump/Content Screens Position (keys.c, dump functions)
// +16 aligns with menu text position (accounting for menu's leading space)
#define UI_CONTENT_START_X (UI_MENU_START_X + 16)  // Aligned with menu text
#define UI_CONTENT_START_Y 32   // Vertical position for dump text
#define UI_CONTENT_PARTIAL_Y 96 // Vertical position for partial keys dump

// Screenshot Notification Position (tui.c)
#define UI_NOTIFY_X        5    // Horizontal position for notifications
#define UI_NOTIFY_Y        680  // Vertical position for notifications (above bottom bar)
```

### Landscape-Specific Calculations

```c
// ============================================================================
// Landscape Mode Text Positioning Template (1280x720 screen, 16x16 rotated font)
// ============================================================================
// Font: 16 pixels per character (in landscape, spaces are 16px wide)
// Screen: 1280px wide × 720px tall

// Margins in pixels (calculated from UI_MENU_START_X/Y)
#define GFX_LANDSCAPE_MARGIN_LEFT_PX      (UI_MENU_START_X + 16)    // Text start position
#define GFX_LANDSCAPE_MARGIN_TOP_PX       (1279 - UI_MENU_START_X)  // Internal Y position

// Margin in character spaces (0 spaces - positioning done via gfx_con_setpos)
#define GFX_LANDSCAPE_MARGIN_SPACES        0

// Full text line template (30 chars + "done in" + timing)
#define GFX_LANDSCAPE_LINE_WIDTH_CHARS   30
```

---

## 4. Title Bar Implementation (gfx.c)

### Function Declaration (gfx.h)

```c
void gfx_draw_title_bar(const char *title);
void gfx_draw_bottom_bar(const char *legend);
```

### Implementation (gfx.c)

```c
void gfx_draw_title_bar(const char *title)
{
    // Save current graphics state
    u8 saved_fillbg = gfx_con.fillbg;
    u32 saved_bgcol = gfx_con.bgcol;

    // Draw TOP BAR (16px tall, dark grey background for dark theme)
    // Internal: draws vertical strip at x=0-15 across all y=0-1279
    // After rotation: appears as 16px horizontal bar at top of landscape display
    for (u32 y = 0; y < 1280; y++)
    {
        for (u32 x = 0; x < 16; x++)
        {
            gfx_ctxt.fb[x + y * gfx_ctxt.stride] = 0xFF3D3D3D;
        }
    }

    // Draw title in top-left corner (cyan text on dark grey)
    gfx_con_setcol(0xFF00D8FF, 1, 0xFF3D3D3D);
    gfx_con_setpos(0, 0);
    gfx_printf("%s", title);

    // Restore graphics state
    gfx_con.fillbg = saved_fillbg;
    gfx_con.bgcol = saved_bgcol;
    gfx_con.fgcol = 0xFFCCCCCC;  // Default text color
}

void gfx_draw_bottom_bar(const char *legend)
{
    // Save current graphics state
    u8 saved_fillbg = gfx_con.fillbg;
    u32 saved_bgcol = gfx_con.bgcol;

    // Draw BOTTOM BAR (16px tall, dark grey background for dark theme)
    // Internal: draws vertical strip at x=704-719 across all y=0-1279
    // After rotation: appears as 16px horizontal bar at bottom of landscape display
    for (u32 y = 0; y < 1280; y++)
    {
        for (u32 x = 704; x < 720; x++)
        {
            gfx_ctxt.fb[x + y * gfx_ctxt.stride] = 0xFF3D3D3D;
        }
    }

    // Draw legend in bottom-left corner (cyan text on dark grey)
    gfx_con_setcol(0xFF00D8FF, 1, 0xFF3D3D3D);
    gfx_con_setpos(0, 704);  // y=704 = 720 - 16 (bottom bar position)
    gfx_printf("%s", legend);

    // Restore graphics state
    gfx_con.fillbg = saved_fillbg;
    gfx_con.bgcol = saved_bgcol;
    gfx_con.fgcol = 0xFFCCCCCC;
}
```

### Key Points

1. **Title bar**: Vertical strip at internal x=0-15 → horizontal bar at top
2. **Bottom bar**: Vertical strip at internal x=704-719 → horizontal bar at bottom
3. **Coordinate mapping**: Internal x → External y (after rotation)
4. **Color scheme**: Dark grey background (0xFF3D3D3D) with cyan text (0xFF00D8FF)

---

## 5. Menu Rendering (tui.c)

### Updated tui_do_menu() Structure

```c
void *tui_do_menu(menu_t *menu)
{
    int idx = 0, prev_idx = -1, cnt = 0x7FFFFFFF;
    int need_full_redraw = 1;
    int last_drawn_idx = -1;

    gfx_clear_grey(0x1B);  // Clear full screen (use instead of partial)
    tui_sbar(true);

    while (true)
    {
        // Skip caption or separator lines selection
        while (menu->ents[idx].type == MENT_CAPTION ||
               menu->ents[idx].type == MENT_CHGLINE)
        {
            // Navigation logic...
        }

        // Handle full redraw (initial or after handler)
        if (need_full_redraw)
        {
            need_full_redraw = 0;

            // Clear screen
            gfx_clear_grey(0x1B);

            // Draw title bar and bottom bar
            char title[64];
            s_printf(title, "[Your App Name v%d.%d.%d]", VER_MJ, VER_MN, VER_BF);
            gfx_draw_title_bar(title);
            gfx_draw_bottom_bar("Joy-Con/Btns: Move   A/Power: Select   Cap+: Screenshot");

            // Draw all menu items using global UI constants
            u32 start_x = UI_MENU_START_X;
            u32 start_y = UI_MENU_START_Y;

            for (cnt = 0; menu->ents[cnt].type != MENT_END; cnt++)
            {
                gfx_con_setpos(start_x, start_y + (cnt * UI_MENU_SPACING));

                // Selection highlighting (inverted colors)
                if (cnt == idx)
                    gfx_con_setcol(0xFF1B1B1B, 1, 0xFFCCCCCC);  // Dark bg, light text
                else
                    gfx_con_setcol(0xFFCCCCCC, 1, 0xFF1B1B1B);  // Light bg, dark text

                if (menu->ents[cnt].type != MENT_CHGLINE)
                {
                    if (cnt == idx)
                        gfx_printf(" %s", menu->ents[cnt].caption);
                    else
                        gfx_printf("%k %s", menu->ents[cnt].color, menu->ents[cnt].caption);
                }
            }
        }

        // Handle input and selection...
    }
}
```

### Key Changes from Portrait

| Aspect | Portrait (Original) | Landscape (Pro) |
|--------|---------------------|-----------------|
| Screen clear | `gfx_clear_partial_grey()` | `gfx_clear_grey()` |
| Menu position | Dynamic with `\n` | Fixed with `gfx_con_setpos()` |
| Spacing | Variable | Fixed 24px (`UI_MENU_SPACING`) |
| Title | Inline with menu | Dedicated `gfx_draw_title_bar()` |
| Help text | At y=1127, 1191 | Dedicated `gfx_draw_bottom_bar()` |

---

## 6. Status Bar / Battery Indicator

### Updated tui_sbar() for Landscape

```c
void tui_sbar(bool force_update)
{
    u32 cx, cy;
    static u32 sbar_time_keeping = 0;

    u32 timePassed = get_tmr_s() - sbar_time_keeping;
    if (!force_update)
        if (timePassed < 5)
            return;

    u8 prevFontSize = gfx_con.fntsz;
    gfx_con.fntsz = 16;
    sbar_time_keeping = get_tmr_s();

    u32 battPercent = 0;

    gfx_con_getpos(&cx, &cy);
    // Position inside bottom bar: x=1050 (right side), y=704 (bottom bar)
    gfx_con_setpos(1050, 704);

    max17050_get_property(MAX17050_RepSOC, (int *)&battPercent);

    // Save graphics state
    u8 saved_fillbg = gfx_con.fillbg;
    u32 saved_bgcol = gfx_con.bgcol;

    // Cyan text on dark grey background
    gfx_printf("%K%k Batt: %d%%", 0xFF3D3D3D, 0xFF00D8FF, battPercent >> 8);

    // Restore graphics state
    gfx_con.fillbg = saved_fillbg;
    gfx_con.bgcol = saved_bgcol;

    gfx_con.fntsz = prevFontSize;
    gfx_con_setpos(cx, cy);
}
```

### Position Mapping

- **Original (Portrait)**: `gfx_con_setpos(0, 1260)` - near bottom
- **Modified (Landscape)**: `gfx_con_setpos(1050, 704)` - inside bottom bar

---

## 7. Color Scheme Reference

### Color Palette

| Element | Color Name | Hex Code | Usage |
|---------|------------|----------|-------|
| Title/Bottom bar bg | Dark grey | `0xFF3D3D3D` | Bar backgrounds |
| Content area bg | Near black | `0xFF1B1B1B` | Main screen background |
| Status bar bg | Dark grey | `0xFF303030` | Battery area |
| Title text | Cyan | `0xFF00D8FF` | Title, legend, battery |
| Menu text (normal) | Light grey | `0xFFCCCCCC` | Menu items |
| Menu text (selected) | Near black | `0xFF1B1B1B` | Selected item background |
| Selected highlight | Light grey | `0xFFCCCCCC` | Selected item text |

### Color Code Format

The color format is **ABGR** (Alpha-Blue-Green-Red):
```c
// 0xAABBGGRR
0xFF00D8FF = Alpha:FF, Blue:00, Green:D8, Red:FF → Cyan
0xFF3D3D3D = Alpha:FF, Blue:3D, Green:3D, Red:3D → Dark grey
```

---

## 8. Quick Reference Checklist

### For New Projects

- [ ] **Display config**: Add `WIN_ENABLE | SCAN_COLUMN | H_DIRECTION` to display init
- [ ] **UI constants**: Add `UI_MENU_START_X/Y`, `UI_MENU_SPACING`, etc. to gfx.h
- [ ] **Title bar**: Implement `gfx_draw_title_bar()` function
- [ ] **Bottom bar**: Implement `gfx_draw_bottom_bar()` function
- [ ] **Menu render**: Update `tui_do_menu()` with fixed positioning
- [ ] **Status bar**: Update `tui_sbar()` for new bottom bar position
- [ ] **Screen clear**: Change `gfx_clear_partial_grey()` to `gfx_clear_grey()`

### Coordinate Quick Reference

| UI Element | Internal X | Internal Y | Display Position |
|------------|------------|------------|-------------------|
| Title bar | 0-15 | 0-1279 | Top (y=0-15) |
| Bottom bar | 704-719 | 0-1279 | Bottom (y=704-719) |
| Menu start | 5 | 32 | Below title bar |
| Battery | 1050 | 704 | Bottom bar right |

---

## 9. File Modification Summary

### Files Typically Modified

| File | Changes |
|------|---------|
| `bdk/display/di.inl` | Hardware rotation flags |
| `bdk/display/di.h` | Flag definitions (if missing) |
| `source/gfx/gfx.h` | UI constants, function declarations |
| `source/gfx/gfx.c` | Title/bottom bar implementations |
| `source/gfx/tui.c` | Menu rendering, status bar |
| `source/frontend/gui.c` | Screen layout adjustments |

---

## 10. Example: Converting Existing Code

### Before (Portrait)

```c
void show_screen(void)
{
    gfx_clear_partial_grey(0x1B, 0, 1200);

    gfx_con_setpos(30, 30);
    gfx_printf("Option 1\n");
    gfx_printf("Option 2\n");
    gfx_printf("Option 3\n");

    gfx_con_setpos(0, 1127);
    gfx_printf("VOL: Move");
}
```

### After (Landscape)

```c
void show_screen(void)
{
    gfx_clear_grey(0x1B);

    // Draw bars
    gfx_draw_title_bar("[My App]");
    gfx_draw_bottom_bar("VOL: Move   A: Select");

    // Menu with fixed positioning
    u32 start_x = UI_MENU_START_X;   // 5
    u32 start_y = UI_MENU_START_Y;   // 32

    gfx_con_setpos(start_x, start_y);
    gfx_printf("Option 1\n");
    gfx_con_setpos(start_x, start_y + UI_MENU_SPACING);  // 32 + 24 = 56
    gfx_printf("Option 2\n");
    gfx_con_setpos(start_x, start_y + (UI_MENU_SPACING * 2));  // 32 + 48 = 80
    gfx_printf("Option 3\n");

    // No manual help text - handled by bottom bar
}
```

---

## Appendix: Display Constants

### Framebuffer Dimensions
- Width: 720 pixels
- Height: 1280 pixels
- Stride: 720 pixels

### Display Dimensions (After Rotation)
- Width: 1280 pixels
- Height: 720 pixels

### Font Dimensions
- Standard: 16×16 pixels
- In landscape: effectively 16×16 (rotated)

### Bar Heights
- Title bar: 16 pixels
- Bottom bar: 16 pixels
- Available content area: 688 pixels (720 - 32)

---

*Template created from Lockpick RCM Pro landscape implementation*
*Compatible with Nintendo Switch homebrew projects using similar display stack*
