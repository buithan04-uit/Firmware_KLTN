#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════════════════════════
    LVGL ICON GENERATOR & FONT INSPECTOR - All-in-One Tool
    Tạo icon fonts cho ESP32 + LVGL từ Material Symbols
═══════════════════════════════════════════════════════════════════════════════

Features:
    🔍 Search icons in Material Symbols font
    ✨ Generate LVGL font files (.c)
    🎨 Auto-update custom_icons.h
    📦 Batch processing
    🎭 Beautiful terminal UI

Requirements:
    pip install fonttools requests

Usage:
    python icon_tool.py                    # Interactive menu
    python icon_tool.py search heart       # Search icons
    python icon_tool.py generate heart e87d # Generate icon
    python icon_tool.py batch icons.txt    # Batch mode

Author: ESP32 Medical Device Project
Version: 2.0
"""

import os
import sys
import re
import subprocess
import requests
from pathlib import Path
from typing import Optional, Tuple, List, Dict

try:
    from fontTools.ttLib import TTFont
    from fontTools.varLib import instancer
    HAS_FONTTOOLS = True
except ImportError:
    HAS_FONTTOOLS = False

# ═══════════════════════════════════════════════════════════════════════════
# 🎨 TERMINAL COLORS & FORMATTING
# ═══════════════════════════════════════════════════════════════════════════

class Colors:
    """ANSI color codes for beautiful terminal output"""
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    UNDERLINE = '\033[4m'
    
    # Colors
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'
    
    # Bright colors
    BRIGHT_RED = '\033[91m'
    BRIGHT_GREEN = '\033[92m'
    BRIGHT_YELLOW = '\033[93m'
    BRIGHT_BLUE = '\033[94m'
    BRIGHT_MAGENTA = '\033[95m'
    BRIGHT_CYAN = '\033[96m'
    BRIGHT_WHITE = '\033[97m'
    
    # Backgrounds
    BG_RED = '\033[41m'
    BG_GREEN = '\033[42m'
    BG_YELLOW = '\033[43m'
    BG_BLUE = '\033[44m'
    BG_CYAN = '\033[46m'

def print_header(text: str, char: str = "═"):
    """Print beautiful header"""
    width = 80
    print(f"\n{Colors.BRIGHT_CYAN}{char * width}{Colors.RESET}")
    print(f"{Colors.BRIGHT_WHITE}{Colors.BOLD}{text.center(width)}{Colors.RESET}")
    print(f"{Colors.BRIGHT_CYAN}{char * width}{Colors.RESET}\n")

def print_success(text: str):
    """Print success message"""
    print(f"{Colors.BRIGHT_GREEN}✅ {text}{Colors.RESET}")

def print_error(text: str):
    """Print error message"""
    print(f"{Colors.BRIGHT_RED}❌ {text}{Colors.RESET}")

def print_warning(text: str):
    """Print warning message"""
    print(f"{Colors.BRIGHT_YELLOW}⚠️  {text}{Colors.RESET}")

def print_info(text: str):
    """Print info message"""
    print(f"{Colors.BRIGHT_BLUE}ℹ️  {text}{Colors.RESET}")

def print_step(number: int, text: str):
    """Print numbered step"""
    print(f"{Colors.BRIGHT_MAGENTA}{number}️⃣  {Colors.WHITE}{text}{Colors.RESET}")

def print_icon_result(unicode_code: str, utf8_code: str, glyph_name: str = ""):
    """Print icon generation result"""
    print(f"{Colors.CYAN}Unicode:{Colors.RESET} {Colors.BRIGHT_WHITE}U+{unicode_code.upper()}{Colors.RESET}")
    print(f"{Colors.CYAN}UTF-8:  {Colors.RESET} {Colors.BRIGHT_YELLOW}{utf8_code}{Colors.RESET}")
    if glyph_name:
        print(f"{Colors.CYAN}Glyph:  {Colors.RESET} {Colors.GREEN}{glyph_name}{Colors.RESET}")

# ═══════════════════════════════════════════════════════════════════════════
# ⚙️  CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════

PROJECT_ROOT = Path(__file__).parent
ICONS_DIR = PROJECT_ROOT / "src" / "icons"
HEADER_FILE = PROJECT_ROOT / "include" / "custom_icons.h"
FONT_FILE = PROJECT_ROOT / "MaterialSymbols-Simple-24.ttf"
FONT_URL = "https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsOutlined%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf"
FONT_SIZE = 24
FONT_BPP = 4

# ═══════════════════════════════════════════════════════════════════════════
# 🔧 CORE FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════

def unicode_to_utf8(hex_code: str) -> Tuple[Optional[str], Optional[str]]:
    """
    Convert Unicode hex to UTF-8 byte string
    
    Args:
        hex_code: Unicode code point (e.g., "e87d", "U+E87D", "0xE87D")
    
    Returns:
        Tuple of (hex_string, c_string) or (None, None) if invalid
    """
    hex_code = hex_code.replace("U+", "").replace("0x", "").replace("u+", "").strip()
    
    try:
        code_point = int(hex_code, 16)
    except ValueError:
        return None, None
    
    if code_point < 0x80:
        # 1-byte UTF-8
        utf8_hex = f"0x{code_point:02X}"
        c_string = f'"\\x{code_point:02X}"'
    elif code_point < 0x800:
        # 2-byte UTF-8
        byte1 = 0xC0 | (code_point >> 6)
        byte2 = 0x80 | (code_point & 0x3F)
        utf8_hex = f"0x{byte1:02X} 0x{byte2:02X}"
        c_string = f'"\\x{byte1:02X}\\x{byte2:02X}"'
    elif code_point < 0x10000:
        # 3-byte UTF-8
        byte1 = 0xE0 | (code_point >> 12)
        byte2 = 0x80 | ((code_point >> 6) & 0x3F)
        byte3 = 0x80 | (code_point & 0x3F)
        utf8_hex = f"0x{byte1:02X} 0x{byte2:02X} 0x{byte3:02X}"
        c_string = f'"\\x{byte1:02X}\\x{byte2:02X}\\x{byte3:02X}"'
    else:
        # 4-byte UTF-8
        byte1 = 0xF0 | (code_point >> 18)
        byte2 = 0x80 | ((code_point >> 12) & 0x3F)
        byte3 = 0x80 | ((code_point >> 6) & 0x3F)
        byte4 = 0x80 | (code_point & 0x3F)
        utf8_hex = f"0x{byte1:02X} 0x{byte2:02X} 0x{byte3:02X} 0x{byte4:02X}"
        c_string = f'"\\x{byte1:02X}\\x{byte2:02X}\\x{byte3:02X}\\x{byte4:02X}"'
    
    return utf8_hex, c_string


def download_font() -> bool:
    """Download Material Symbols font if not exists"""
    if FONT_FILE.exists():
        return True
    
    print_info(f"Downloading font from GitHub...")
    try:
        response = requests.get(FONT_URL, timeout=30)
        response.raise_for_status()
        
        FONT_FILE.parent.mkdir(parents=True, exist_ok=True)
        FONT_FILE.write_bytes(response.content)
        
        print_success(f"Downloaded: {FONT_FILE.name}")
        return True
    except Exception as e:
        print_error(f"Download failed: {e}")
        return False


def convert_variable_font(input_font: str, output_font: str, 
                         fill: int = 0, grad: int = 0, 
                         opsz: int = 24, wght: int = 400) -> bool:
    """
    Convert Material Symbols variable font to static instance
    
    Args:
        input_font: Path to variable font
        output_font: Path to save static font
        fill: FILL axis (0=outlined, 1=filled)
        grad: GRAD axis (-50 to 200)
        opsz: Optical size (20-48)
        wght: Weight (100-700)
    
    Returns:
        True if successful, False otherwise
    """
    if not HAS_FONTTOOLS:
        print_error("fontTools not installed! Run: pip install fonttools")
        return False
    
    try:
        print_info(f"Loading variable font: {input_font}")
        font = TTFont(input_font)
        
        # Check if font is variable
        if 'fvar' not in font:
            print_error("Font is not a variable font!")
            font.close()
            return False
        
        # Display available axes
        fvar = font['fvar']
        print(f"\n{Colors.CYAN}📊 Available axes:{Colors.RESET}")
        for axis in fvar.axes:
            print(f"   • {Colors.YELLOW}{axis.axisTag}{Colors.RESET}: "
                  f"{axis.minValue} - {axis.maxValue} "
                  f"(default: {axis.defaultValue})")
        
        # Prepare axes values
        axes_values = {
            'FILL': fill,
            'GRAD': grad,
            'opsz': opsz,
            'wght': wght
        }
        
        # Display requested values
        print(f"\n{Colors.CYAN}🎯 Creating static instance:{Colors.RESET}")
        for tag, value in axes_values.items():
            print(f"   • {Colors.YELLOW}{tag}{Colors.RESET} = {Colors.GREEN}{value}{Colors.RESET}")
        
        # Create static instance
        print(f"\n{Colors.CYAN}⚙️  Converting to static font...{Colors.RESET}")
        instancer.instantiateVariableFont(font, axes_values)
        
        # Save static font
        print(f"{Colors.CYAN}💾 Saving to: {Colors.RESET}{output_font}")
        font.save(output_font)
        font.close()
        
        print_success("Static font created successfully!")
        return True
        
    except Exception as e:
        print_error(f"Conversion failed: {e}")
        return False


def check_lv_font_conv() -> bool:
    """Check if lv_font_conv is installed"""
    try:
        result = subprocess.run(
            ["lv_font_conv", "--help"],
            capture_output=True,
            text=True,
            shell=True  # Windows compatibility
        )
        return result.returncode == 0
    except FileNotFoundError:
        return False


def generate_font_file(icon_name: str, hex_code: str) -> Optional[str]:
    """
    Generate LVGL font file using lv_font_conv
    
    Args:
        icon_name: Name of the icon (e.g., "heart")
        hex_code: Unicode hex code (e.g., "e87d")
    
    Returns:
        Path to generated file or None if failed
    """
    if not download_font():
        return None
    
    if not check_lv_font_conv():
        print_error("lv_font_conv not found!")
        print_info("Install: npm install -g lv_font_conv")
        return None
    
    ICONS_DIR.mkdir(parents=True, exist_ok=True)
    
    font_filename = f"lv_font_{icon_name.lower()}_24.c"
    output_file = ICONS_DIR / font_filename
    
    # Ensure hex code has 0x prefix
    if not hex_code.startswith("0x"):
        hex_code = f"0x{hex_code}"
    
    cmd = [
        "lv_font_conv",
        "--font", str(FONT_FILE),
        "--size", str(FONT_SIZE),
        "--bpp", str(FONT_BPP),
        "--format", "lvgl",
        "--range", hex_code,  # Use --range for hex codepoints, not --symbols
        "--no-compress",
        "-o", str(output_file)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            shell=True  # Windows compatibility
        )
        
        if result.returncode == 0 and output_file.exists():
            print_success(f"Generated: {font_filename}")
            return str(output_file)
        else:
            print_error(f"Generation failed: {result.stderr}")
            return None
    except Exception as e:
        print_error(f"Exception: {e}")
        return None


def update_header_file(icon_name: str, hex_code: str, font_file: str, force: bool = False) -> bool:
    """
    Update custom_icons.h with new icon
    
    Args:
        icon_name: Name of the icon
        hex_code: Unicode hex code
        font_file: Path to generated font file
        force: Force overwrite without asking
    
    Returns:
        True if successful
    """
    _, utf8_string = unicode_to_utf8(hex_code)
    if not utf8_string:
        print_error("Invalid Unicode code")
        return False
    
    HEADER_FILE.parent.mkdir(parents=True, exist_ok=True)
    
    icon_upper = icon_name.upper()
    font_name = f"lv_font_{icon_name.lower()}_24"
    
    # Create header template if not exists
    if not HEADER_FILE.exists():
        header_content = """/*******************************************************************************
 * Custom Icons Header - Auto-generated by icon_tool.py
 * DO NOT EDIT MANUALLY - Use icon_tool.py to add new icons
 ******************************************************************************/

#ifndef CUSTOM_ICONS_H
#define CUSTOM_ICONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

/*-----------------------------------------------------------------------------
 * FONT DECLARATIONS
 *---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * ICON DEFINITIONS (UTF-8 Encoded)
 *---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_ICONS_H
"""
        HEADER_FILE.write_text(header_content)
    
    content = HEADER_FILE.read_text()
    
    # Check if icon already exists
    if f"ICON_{icon_upper}" in content and not force:
        print_warning(f"Icon {icon_upper} already exists")
        response = input(f"   {Colors.YELLOW}Overwrite? (y/n):{Colors.RESET} ").strip().lower()
        if response != 'y':
            print_info("Skipped header update")
            return False
    
    # Remove old declarations (both font and icon define)
    content = re.sub(
        rf"^\s*// {icon_upper}.*\n\s*LV_FONT_DECLARE\({font_name}\);\n",
        "",
        content,
        flags=re.MULTILINE
    )
    content = re.sub(
        rf"^// {icon_upper}.*\n#define ICON_{icon_upper}.*\n",
        "",
        content,
        flags=re.MULTILINE
    )
    
    # Find and add font declaration
    font_section_pattern = r"(/\*-+\s*\n \* FONT DECLARATIONS\s*\n \*-+\*/)"
    match = re.search(font_section_pattern, content)
    
    if match:
        insert_pos = match.end()
        font_decl = f"\n// {icon_upper} icon font\nLV_FONT_DECLARE({font_name});\n"
        content = content[:insert_pos] + font_decl + content[insert_pos:]
    else:
        print_warning("Could not find FONT DECLARATIONS section")
    
    # Find and add icon definition
    icon_section_pattern = r"(/\*-+\s*\n \* ICON DEFINITIONS \(UTF-8 Encoded\)\s*\n \*-+\*/)"
    match = re.search(icon_section_pattern, content)
    
    if match:
        insert_pos = match.end()
        hex_upper = hex_code.replace("0x", "").upper()
        icon_def = f"\n// {icon_upper} icon (U+{hex_upper})\n#define ICON_{icon_upper} {utf8_string}\n"
        content = content[:insert_pos] + icon_def + content[insert_pos:]
    else:
        print_warning("Could not find ICON DEFINITIONS section")
    
    HEADER_FILE.write_text(content)
    print_success(f"Updated: {HEADER_FILE.name}")
    return True


# ═══════════════════════════════════════════════════════════════════════════
# 🔍 FONT INSPECTION
# ═══════════════════════════════════════════════════════════════════════════

def search_font(search_term: str, max_results: int = 50) -> List[Dict[str, str]]:
    """
    Search for glyphs in font
    
    Args:
        search_term: Search query (glyph name)
        max_results: Maximum results to return
    
    Returns:
        List of dicts with 'code', 'hex', 'name' keys
    """
    if not HAS_FONTTOOLS:
        print_error("fonttools not installed!")
        print_info("Install: pip install fonttools")
        return []
    
    if not FONT_FILE.exists():
        if not download_font():
            return []
    
    try:
        font = TTFont(FONT_FILE)
        cmap = font.getBestCmap()
        
        if not cmap:
            print_error("No character map found in font")
            return []
        
        results = []
        search_lower = search_term.lower() if search_term else None
        
        for code_point, glyph_name in sorted(cmap.items()):
            if search_lower and search_lower not in glyph_name.lower():
                continue
            
            results.append({
                'code': code_point,
                'hex': f"{code_point:04X}",
                'name': glyph_name
            })
            
            if len(results) >= max_results:
                break
        
        font.close()
        return results
    
    except Exception as e:
        print_error(f"Font reading error: {e}")
        return []


def display_search_results(results: List[Dict[str, str]], query: str):
    """Display search results in beautiful format"""
    if not results:
        print_warning(f"No icons found for: {query}")
        return
    
    print_header(f"🔍 SEARCH RESULTS: {query}")
    print(f"{Colors.BRIGHT_WHITE}Found {len(results)} icons:{Colors.RESET}\n")
    
    # Table header
    print(f"{Colors.CYAN}{'No.':<5} {'Unicode':<10} {'Hex Code':<10} {'Glyph Name'}{Colors.RESET}")
    print(f"{Colors.DIM}{'─' * 80}{Colors.RESET}")
    
    # Results
    for idx, item in enumerate(results, 1):
        print(f"{Colors.YELLOW}{idx:<5}{Colors.RESET} "
              f"{Colors.BRIGHT_WHITE}U+{item['hex']:<8}{Colors.RESET} "
              f"{Colors.GREEN}0x{item['hex']:<8}{Colors.RESET} "
              f"{Colors.WHITE}{item['name']}{Colors.RESET}")
    
    print(f"\n{Colors.BRIGHT_CYAN}Total: {len(results)} glyphs{Colors.RESET}")


# ═══════════════════════════════════════════════════════════════════════════
# 🎯 ICON GENERATION WORKFLOW
# ═══════════════════════════════════════════════════════════════════════════

def process_icon(icon_name: str, hex_code: str, force: bool = False) -> bool:
    """
    Complete workflow to generate icon
    
    Args:
        icon_name: Name of the icon
        hex_code: Unicode hex code
        force: Force overwrite without prompting
    
    Returns:
        True if successful
    """
    print_header(f"🎨 GENERATING ICON: {icon_name.upper()}")
    
    # Validate inputs
    icon_name = icon_name.strip().lower()
    hex_code = hex_code.strip().replace("U+", "").replace("0x", "")
    
    if not re.match(r'^[a-z0-9_]+$', icon_name):
        print_error("Icon name must be alphanumeric with underscores")
        return False
    
    if not re.match(r'^[0-9a-fA-F]+$', hex_code):
        print_error("Hex code must be valid hexadecimal")
        return False
    
    # Convert to UTF-8
    _, utf8 = unicode_to_utf8(hex_code)
    if not utf8:
        print_error("Invalid Unicode code point")
        return False
    
    print_icon_result(hex_code, utf8)
    print()
    
    # Generate font file
    print_step(1, "Generating LVGL font file...")
    font_file = generate_font_file(icon_name, hex_code)
    if not font_file:
        return False
    
    # Update header
    print_step(2, "Updating custom_icons.h...")
    if not update_header_file(icon_name, hex_code, font_file, force=force):
        print_warning("Header not updated, but font file was generated")
    
    print_header("✅ ICON GENERATION COMPLETE")
    if not force:
        print_usage_example(icon_name)
    return True


def print_usage_example(icon_name: str):
    """Print usage example"""
    icon_upper = icon_name.upper()
    print(f"{Colors.BRIGHT_CYAN}Usage in ui.cpp:{Colors.RESET}\n")
    
    print(f"{Colors.DIM}// Include header{Colors.RESET}")
    print(f"{Colors.WHITE}#include \"custom_icons.h\"{Colors.RESET}\n")
    
    print(f"{Colors.DIM}// Create label with icon{Colors.RESET}")
    print(f"{Colors.WHITE}lv_obj_t *icon = lv_label_create(parent);{Colors.RESET}")
    print(f"{Colors.WHITE}lv_label_set_text(icon, ICON_{icon_upper});{Colors.RESET}")
    print(f"{Colors.WHITE}lv_obj_set_style_text_font(icon, &lv_font_{icon_name.lower()}_24, 0);{Colors.RESET}")
    print(f"{Colors.WHITE}lv_obj_set_style_text_color(icon, lv_color_hex(0xFF0000), 0);{Colors.RESET}\n")


# ═══════════════════════════════════════════════════════════════════════════
# 📦 BATCH PROCESSING
# ═══════════════════════════════════════════════════════════════════════════

def batch_process(batch_file: str) -> bool:
    """
    Process multiple icons from file
    
    File format (each line):
        icon_name hex_code
    
    Example:
        heart e87d
        wifi e63e
        settings e8b8
    """
    batch_path = Path(batch_file)
    if not batch_path.exists():
        print_error(f"Batch file not found: {batch_file}")
        return False
    
    print_header(f"📦 BATCH PROCESSING: {batch_path.name}")
    
    lines = batch_path.read_text().splitlines()
    icons = []
    
    for line_num, line in enumerate(lines, 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        
        parts = line.split()
        if len(parts) != 2:
            print_warning(f"Line {line_num}: Invalid format, expected: name hex_code")
            continue
        
        icons.append(tuple(parts))
    
    if not icons:
        print_warning("No valid icons found in batch file")
        return False
    
    print_info(f"Found {len(icons)} icons to process\n")
    
    success = 0
    failed = 0
    
    for idx, (name, code) in enumerate(icons, 1):
        print(f"\n{Colors.BRIGHT_MAGENTA}[{idx}/{len(icons)}]{Colors.RESET}")
        if process_icon(name, code, force=True):  # Force in batch mode
            success += 1
        else:
            failed += 1
    
    print_header("📊 BATCH SUMMARY")
    print(f"{Colors.BRIGHT_GREEN}✅ Success: {success}{Colors.RESET}")
    if failed > 0:
        print(f"{Colors.BRIGHT_RED}❌ Failed:  {failed}{Colors.RESET}")
    
    return failed == 0


# ═══════════════════════════════════════════════════════════════════════════
# 🎮 INTERACTIVE MODE
# ═══════════════════════════════════════════════════════════════════════════

def interactive_mode():
    """Interactive menu-driven mode"""
    # Welcome banner
    print(f"\n{Colors.BRIGHT_CYAN}{'═' * 80}{Colors.RESET}")
    print(f"{Colors.BRIGHT_WHITE}{Colors.BOLD}{'🎨 LVGL ICON TOOL v2.0'.center(80)}{Colors.RESET}")
    print(f"{Colors.BRIGHT_CYAN}{'Material Symbols Font Generator for ESP32 + LVGL'.center(80)}{Colors.RESET}")
    print(f"{Colors.BRIGHT_CYAN}{'═' * 80}{Colors.RESET}\n")
    
    while True:
        print_header("🎮 MAIN MENU")
        
        print(f"{Colors.BRIGHT_WHITE}Choose an option:{Colors.RESET}\n")
        print(f"  {Colors.BRIGHT_CYAN}1.{Colors.RESET} {Colors.WHITE}🔍 Search icons in font{Colors.RESET}")
        print(f"  {Colors.BRIGHT_CYAN}2.{Colors.RESET} {Colors.WHITE}✨ Generate icon{Colors.RESET}")
        print(f"  {Colors.BRIGHT_CYAN}3.{Colors.RESET} {Colors.WHITE}📦 Batch processing{Colors.RESET}")
        print(f"  {Colors.BRIGHT_CYAN}4.{Colors.RESET} {Colors.WHITE}🔄 Convert variable font{Colors.RESET}")
        print(f"  {Colors.BRIGHT_CYAN}5.{Colors.RESET} {Colors.WHITE}ℹ️  System info{Colors.RESET}")
        print(f"  {Colors.BRIGHT_CYAN}0.{Colors.RESET} {Colors.WHITE}🚪 Exit{Colors.RESET}\n")
        
        choice = input(f"{Colors.BRIGHT_YELLOW}Enter choice [0-5]:{Colors.RESET} ").strip()
        
        if choice == "1":
            # Search mode
            query = input(f"\n{Colors.CYAN}Search query:{Colors.RESET} ").strip()
            if query:
                results = search_font(query)
                display_search_results(results, query)
                
                if results:
                    gen = input(f"\n{Colors.YELLOW}Generate from results? (y/n):{Colors.RESET} ").strip().lower()
                    if gen == 'y':
                        try:
                            idx = int(input(f"{Colors.CYAN}Enter number (1-{len(results)}):{Colors.RESET} "))
                            if 1 <= idx <= len(results):
                                item = results[idx - 1]
                                name = input(f"{Colors.CYAN}Icon name:{Colors.RESET} ").strip()
                                if name:
                                    process_icon(name, item['hex'])
                        except ValueError:
                            print_error("Invalid number")
            
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        
        elif choice == "2":
            # Generate mode
            print()
            name = input(f"{Colors.CYAN}Icon name:{Colors.RESET} ").strip()
            code = input(f"{Colors.CYAN}Hex code (e.g., e87d):{Colors.RESET} ").strip()
            
            if name and code:
                process_icon(name, code)
            else:
                print_error("Name and code are required")
            
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        
        elif choice == "3":
            # Batch mode
            print()
            file_path = input(f"{Colors.CYAN}Batch file path:{Colors.RESET} ").strip()
            if file_path:
                batch_process(file_path)
            
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        
        elif choice == "4":
            # Convert variable font
            print_header("🔄 CONVERT VARIABLE FONT")
            
            print(f"{Colors.CYAN}This converts Material Symbols variable font to static instance{Colors.RESET}")
            print(f"{Colors.DIM}Optimized for 24px outlined icons{Colors.RESET}\n")
            
            input_font = input(f"{Colors.CYAN}Variable font path:{Colors.RESET} ").strip() or \
                        "MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf"
            output_font = input(f"{Colors.CYAN}Output path:{Colors.RESET} ").strip() or \
                         "MaterialSymbols-Simple-24.ttf"
            
            print(f"\n{Colors.YELLOW}Font style (press Enter for defaults):{Colors.RESET}")
            try:
                fill = int(input(f"{Colors.DIM}FILL [0=outlined, 1=filled] (0):{Colors.RESET} ").strip() or "0")
                wght = int(input(f"{Colors.DIM}Weight [100-700] (400):{Colors.RESET} ").strip() or "400")
                opsz = int(input(f"{Colors.DIM}Optical size [20-48] (24):{Colors.RESET} ").strip() or "24")
                grad = int(input(f"{Colors.DIM}Gradient [-50-200] (0):{Colors.RESET} ").strip() or "0")
            except ValueError:
                fill, wght, opsz, grad = 0, 400, 24, 0
            
            print()
            convert_variable_font(input_font, output_font, fill, grad, opsz, wght)
            
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        
        elif choice == "5":
            # System info
            print_header("ℹ️  SYSTEM INFORMATION")
            
            print(f"{Colors.CYAN}Project root:{Colors.RESET} {PROJECT_ROOT}")
            print(f"{Colors.CYAN}Icons folder:{Colors.RESET} {ICONS_DIR}")
            print(f"{Colors.CYAN}Header file: {Colors.RESET} {HEADER_FILE}")
            print(f"{Colors.CYAN}Font file:   {Colors.RESET} {FONT_FILE}")
            
            print(f"\n{Colors.BRIGHT_WHITE}Status:{Colors.RESET}")
            print(f"  Font exists:      {'✅ Yes' if FONT_FILE.exists() else '❌ No'}")
            print(f"  lv_font_conv:     {'✅ Yes' if check_lv_font_conv() else '❌ No'}")
            print(f"  fonttools:        {'✅ Yes' if HAS_FONTTOOLS else '❌ No'}")
            
            if FONT_FILE.exists() and HAS_FONTTOOLS:
                try:
                    font = TTFont(FONT_FILE)
                    cmap = font.getBestCmap()
                    print(f"  Total glyphs:     {len(cmap)}")
                    font.close()
                except:
                    pass
            
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        
        elif choice == "0":
            print(f"\n{Colors.BRIGHT_GREEN}Goodbye! 👋{Colors.RESET}\n")
            break
        
        else:
            print_error("Invalid choice")
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")


# ═══════════════════════════════════════════════════════════════════════════
# 🚀 MAIN ENTRY POINT
# ═══════════════════════════════════════════════════════════════════════════

def print_help():
    """Print help message"""
    print_header("🎨 LVGL ICON TOOL - HELP")
    
    print(f"{Colors.BRIGHT_WHITE}Usage:{Colors.RESET}\n")
    
    print(f"  {Colors.CYAN}Interactive mode:{Colors.RESET}")
    print(f"    {Colors.WHITE}python icon_tool.py{Colors.RESET}\n")
    
    print(f"  {Colors.CYAN}Search icons:{Colors.RESET}")
    print(f"    {Colors.WHITE}python icon_tool.py search <query>{Colors.RESET}")
    print(f"    {Colors.DIM}Example: python icon_tool.py search heart{Colors.RESET}\n")
    
    print(f"  {Colors.CYAN}Generate icon:{Colors.RESET}")
    print(f"    {Colors.WHITE}python icon_tool.py generate <name> <hex_code>{Colors.RESET}")
    print(f"    {Colors.DIM}Example: python icon_tool.py generate heart e87d{Colors.RESET}\n")
    
    print(f"  {Colors.CYAN}Batch processing:{Colors.RESET}")
    print(f"    {Colors.WHITE}python icon_tool.py batch <file>{Colors.RESET}")
    print(f"    {Colors.DIM}Example: python icon_tool.py batch icons.txt{Colors.RESET}\n")
    
    print(f"{Colors.BRIGHT_WHITE}Batch file format:{Colors.RESET}")
    print(f"  {Colors.DIM}# Comments start with #{Colors.RESET}")
    print(f"  {Colors.WHITE}heart e87d{Colors.RESET}")
    print(f"  {Colors.WHITE}wifi e63e{Colors.RESET}")
    print(f"  {Colors.WHITE}settings e8b8{Colors.RESET}\n")


def main():
    """Main entry point"""
    args = sys.argv[1:]
    
    if not args or args[0] in ["--help", "-h", "help"]:
        print_help()
        return
    
    command = args[0].lower()
    
    if command == "search":
        if len(args) < 2:
            print_error("Search query required")
            print_info("Usage: python icon_tool.py search <query>")
            return
        
        query = " ".join(args[1:])
        results = search_font(query)
        display_search_results(results, query)
    
    elif command in ["generate", "gen"]:
        if len(args) < 3:
            print_error("Icon name and hex code required")
            print_info("Usage: python icon_tool.py generate <name> <hex_code>")
            return
        
        process_icon(args[1], args[2])
    
    elif command == "batch":
        if len(args) < 2:
            print_error("Batch file path required")
            print_info("Usage: python icon_tool.py batch <file>")
            return
        
        batch_process(args[1])
    
    elif command in ["interactive", "i"]:
        interactive_mode()
    
    else:
        print_error(f"Unknown command: {command}")
        print_info("Run with --help for usage information")


if __name__ == "__main__":
    try:
        # Default to interactive mode if no arguments
        if len(sys.argv) == 1:
            interactive_mode()
        else:
            main()
    except KeyboardInterrupt:
        print(f"\n\n{Colors.YELLOW}⚠️  Interrupted by user{Colors.RESET}\n")
        sys.exit(1)
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
