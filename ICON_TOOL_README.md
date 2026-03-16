# 🎨 LVGL Icon Tool - Material Symbols Generator

All-in-one Python tool for generating Material Symbols icons for ESP32 + LVGL projects.

## ✨ Features

- 🔍 **Search Icons**: Search 4200+ Material Symbols icons by name
- ✨ **Generate Fonts**: Auto-generate LVGL font files (.c)
- 🎨 **Auto Headers**: Update custom_icons.h automatically
- 📦 **Batch Processing**: Generate multiple icons at once
- 🔄 **Font Converter**: Convert variable fonts to static instances
- 🎭 **Beautiful UI**: Colorful terminal interface with emojis

## 📋 Requirements

### Python Dependencies

```bash
pip install fonttools requests
```

### Node.js Tool

```bash
npm install -g lv_font_conv
```

## 🚀 Quick Start

### Interactive Mode (Recommended)

```bash
python icon_tool.py
```

Menu xuất hiện với các options:

1. 🔍 Search icons
2. ✨ Generate icon
3. 📦 Batch processing
4. 🔄 Convert variable font
5. ℹ️ System info
6. 🚪 Exit

### Command Line Mode

**Search icons:**

```bash
python icon_tool.py search heart
python icon_tool.py search wifi
python icon_tool.py search battery
```

**Generate single icon:**

```bash
python icon_tool.py generate heart e87d
python icon_tool.py generate wifi e63e
```

**Batch processing:**

```bash
python icon_tool.py batch icons_regenerate.txt
```

Batch file format (name + hex code):

```
monitor ef5b
heart e87d
wifi e63e
battery f30c
```

## 📁 Project Structure

```
ESP32/
├── icon_tool.py                    # Main tool (all-in-one)
├── icons_regenerate.txt            # Batch file (9 icons)
├── requirements.txt                # Python dependencies
├── MaterialSymbols-Simple-24.ttf   # Static font (optimized 24px)
├── MaterialSymbolsOutlined[...].ttf # Variable font (source)
├── include/
│   └── custom_icons.h              # Auto-generated header
└── src/
    └── icons/
        ├── lv_font_monitor_24.c
        ├── lv_font_heart_24.c
        └── ... (9 icon files)
```

## 🔍 How to Find Icons

### Method 1: Interactive Search

```bash
python icon_tool.py
# Choose option 1
# Enter search term: "heart"
# Choose from results
```

### Method 2: Command Line Search

```bash
python icon_tool.py search heart
```

Output:

```
════════════════════════════════════════════════════════════
                    🔍 SEARCH RESULTS: heart

No.   Unicode    Hex Code   Glyph Name
────────────────────────────────────────────────────────────
1     U+E87D     0xE87D     favorite
2     U+EAA2     0xEAA2     monitor_heart
3     U+EB82     0xEB82     cardiology
...
```

### Method 3: Google Material Symbols Website

1. Visit: https://fonts.google.com/icons
2. Search icon name
3. Copy codepoint (e.g., `e87d`)
4. Generate: `python icon_tool.py generate heart e87d`

## 🎨 Usage in Code

After generation, use icons in your LVGL code:

```cpp
#include "custom_icons.h"

// Create label with icon
lv_obj_t* icon = lv_label_create(parent);
lv_label_set_text(icon, ICON_HEART);
lv_obj_set_style_text_font(icon, &lv_font_heart_24, 0);
lv_obj_set_style_text_color(icon, lv_color_hex(0xFF1744), 0);
```

## 🔄 Converting Variable Fonts

If you need different font styles (filled, bold, etc.):

### Interactive Mode:

```bash
python icon_tool.py
# Choose option 4
# Follow prompts
```

### Font Style Options:

- **FILL**: 0 (outlined) or 1 (filled)
- **Weight**: 100-700 (100=thin, 400=regular, 700=bold)
- **Optical Size**: 20-48 (optimized size)
- **Gradient**: -50 to 200 (weight gradient)

Example: Bold filled icons for 48px

```
FILL = 1
Weight = 700
Optical size = 48
Gradient = 0
```

## 📊 Current Icons (9 total)

| Icon       | Codepoint | Usage                  |
| ---------- | --------- | ---------------------- |
| Monitor    | `0xEF5B`  | Main menu              |
| ECG        | `0xF80F`  | Main menu              |
| Oxygen     | `0xE4DE`  | Main menu, SpO2 screen |
| Lung       | `0xE127`  | Main menu              |
| WiFi       | `0xE63E`  | Main menu, header bar  |
| Config     | `0xE8B8`  | Main menu              |
| Heart      | `0xE87D`  | SpO2 screen            |
| Battery    | `0xF30C`  | Header bar             |
| IP Address | `0xE016`  | Config screen          |

## 🔧 Troubleshooting

### Icons show as rectangles

✅ **FIXED!** Use `--range` instead of `--symbols` in lv_font_conv

```bash
# ❌ Wrong (generates fallback glyphs)
lv_font_conv --symbols ef5b

# ✅ Correct (generates real icon)
lv_font_conv --range 0xef5b
```

### Font not found

```bash
# Auto-download from GitHub
python icon_tool.py
# Or manually download MaterialSymbolsOutlined[...].ttf
```

### lv_font_conv not found

```bash
npm install -g lv_font_conv
```

### fonttools not found

```bash
pip install fonttools requests
```

## 📝 Notes

- **Font size**: Fixed at 24px (optimized for 320x240 TFT)
- **Anti-aliasing**: 4-bit grayscale
- **Style**: Outlined (FILL=0), Regular weight (400)
- **Format**: LVGL 8.x compatible
- **Auto-generated files**:
  - `src/icons/lv_font_*.c` (font data)
  - `include/custom_icons.h` (declarations + macros)

## 🎯 Best Practices

1. **Search first** - Use search to find correct codepoint
2. **Batch processing** - Generate multiple icons at once
3. **Keep batch file** - Save `icons_regenerate.txt` for rebuilds
4. **Test on device** - Verify icons display correctly
5. **Check memory** - Each icon ~4-5KB RAM

## 📚 Resources

- Material Symbols: https://fonts.google.com/icons
- LVGL Font Converter: https://github.com/lvgl/lv_font_conv
- Font Tools: https://github.com/fonttools/fonttools

## 🆘 Support

Issues? Check:

1. Serial Monitor for font loading errors
2. Heap memory (need ~4.5KB per icon)
3. `lv_conf.h` for LV_FONT_CUSTOM_DECLARE
4. Build output for compilation errors

---

**Version**: 2.0  
**Author**: ESP32 Medical Device Project  
**License**: MIT
