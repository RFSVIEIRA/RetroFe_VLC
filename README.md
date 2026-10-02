# RetroFeVLC — Next-Generation Retro Gaming Frontend with 100% Full GUI Suite

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-brightgreen.svg)]()
[![Engine](https://img.shields.io/badge/Engine-VLC%20Media%20Library%204K-orange.svg)]()
[![Configuration](https://img.shields.io/badge/Configuration-100%25%20GUI%20(No%20Text%20Editing)-success.svg)]()
[![Languages](https://img.shields.io/badge/Languages-10%20Built--in-purple.svg)]()

> **🌐 Official Website**: [retrofevlc.eu](https://retrofevlc.eu/) • **📖 Wiki**: [retrofevlc.infy.uk](https://retrofevlc.infy.uk/) • **💬 Forum**: [retrofevlc.infy.uk/forum](https://retrofevlc.infy.uk/forum) • **🎮 Discord**: [Join Discord](https://discord.gg/2aYRHdUrVf)

---

## ⚡ Does RetroFeVLC Have a GUI?
**YES — 100% Complete Graphical User Interface (GUI) for ALL configurations!**

A common misconception inherited from legacy RetroFE (which had **NO graphical configuration interface** and required users to edit complex `.conf` and `.xml` files by hand in text editors) is that RetroFE-based frontends lack a GUI.

**RetroFeVLC completely solves this limitation.** RetroFeVLC includes an all-in-one companion suite of visual desktop applications and graphical editors that allow you to configure **emulators, ROM paths, collections, menus, game filters, playlists, layouts, and gamepads entirely via GUI** — with **zero manual text editing**.

---

## 🌟 The RetroFeVLC GUI Suite

| Tool | Type | Key Capabilities |
| :--- | :--- | :--- |
| **📁 Collections Manager** | **Full GUI Suite** | Complete visual management of collections: configure emulators and executable paths, organize sub-collections, set artwork directories, build playlists and Jukebox queues, and control include/exclude game filters for your wheel. |
| **🚀 Start.exe (Starter)** | **Central GUI** | System launcher, zero-config gamepad auto-mapping, auto-start synchronization, display selector, and system tray management. |
| **📋 MenuManager** | **Visual GUI** | Drag-and-drop menu editor to create, reorder, group, and structure categories and playlists without touching XML files. |
| **🖌️ Layout Editor** | **Visual GUI** | Real-time visual theme designer with live graphical preview: adjust layers, video windows, fonts, wheels, and artwork positions without launching the frontend. |
| **🌳 Layout Diagram Tree** | **Diagnostic & Compiler** | Interactive MSAGL visual dependency graph of modular XML `<include>` files, variable override timeline tracker, and single-file `temp.xml` compiler. |
| **🖼️ RetroFeVLCSkraper** | **Scraper GUI** | Universal multi-service artwork scraper integrating 6 APIs (ScreenScraper.fr, SteamGridDB, EmuMovies, TheGamesDB, IGDB, Libretro CDN) with multi-language metadata downloads. |
| **🔍 Search UI (F11)** | **In-Game GUI** | Ultra-fast library search with controller support, instant title search, system filtering, and game genre classification. |
| **🎨 GenreFixer** | **AI / Batch GUI** | Automatic genre tag normalization and bulk metadata correction. |
| **🌐 Story Translator** | **AI GUI** | Automatic multi-language translation for game synopses and descriptions. |
| **🛡️ KioskMode** | **Console Lock GUI** | Turns Windows into a dedicated arcade console with auto-relaunch and password protection. |
| **📺 Marquee MonitorDuplicator** | **Dual Screen GUI** | Dynamic marquee screen mirroring and secondary display management via visual JSON controls. |
| **🏆 RetroRa Sync** | **Achievement GUI** | Real-time RetroAchievements progress tracking and player stats display. |

---

## 📊 Comparison: RetroFeVLC vs Legacy RetroFE & Competitors

| Feature / Capability | **RetroFeVLC (2026)** | **RetroFE (Legacy)** | **RetroBat** | **LaunchBox / BigBox** |
| :--- | :---: | :---: | :---: | :---: |
| **Configuration GUI** | **100% Dedicated GUI Suite (No Text Editing)** | ❌ No GUI (Manual `.conf` & `.xml` editing) | ⚠️ Partial (EmulationStation menu) | ✅ Yes (Desktop UI) |
| **Cost / License** | **100% Free & Open Source (GPLv3)** | Free & Open Source (GPLv3) | Free | 💲 Freemium ($75 Lifetime) |
| **Video Playback Engine** | **VLC Media Library (4K Pre-Scale)** | GStreamer (Legacy) | FFmpeg / VLC | Windows Media / VLC |
| **Scrolling Speed** | **1 → 2,350 games in 60s** | Moderate | Moderate | Moderate / Resource Heavy |
| **Collection Management** | **Visual Collections Manager GUI** | Manual folder & text files | Internal ES menus | Desktop Wizard |
| **Theme / Layout Creation** | **Visual GUI + Real-Time Preview** | Text editor only | XML code | Advanced (Paid BigBox) |
| **Artwork Scraper** | **RetroFeVLCSkraper (6 Integrated APIs)** | None (External third-party) | Built-in ES scraper | Built-in scraper |
| **Gamepad Setup** | **Zero-Config Auto-Mapping** | Manual mapping | Auto-mapped | Setup Wizard |
| **Jukebox Mode** | **Yes (Up to 32 lists per collection)** | Basic | No | No |
| **Portability** | **100% Portable (Zero Registry Footprint)** | 100% Portable | Portable | Partially Portable |
| **Multi-Language UI** | **10 Languages Built-in** | English Only | Multi-language | Multi-language |

---

## 🚀 Key Architectural Upgrades (From RetroFE to RetroFeVLC)

1. **LibVLC Video Core**:
   - Replaced legacy GStreamer with **VLC Media Library (LibVLC)** for flawless 4K pre-scale rendering, zero memory leaks, and rock-solid arcade cabinet stability.
2. **Unmatched Performance**:
   - Engineered to scroll through **1 to 2,350 games in just 60 seconds** without lag, frame drops, or micro-stutters.
3. **Animated Media Support**:
   - Native support for animated GIFs and modern WebP formats in wheels, backgrounds, and marquees.
4. **Rich Metadata & Sorting**:
   - Built-in metadata fields: `Artist`, `Album`, `Track Number`, `Disc Number`, `Genre`, and `Year`.
   - One-click sorting by Year, Manufacturer, or Genre directly from the interface.
5. **Multi-Display & Marquee Support**:
   - Independent layout configuration per display, layer 20 background rendering, and secondary screen idle animations.
6. **10 Built-In UI Languages**:
   - English, Portuguese, Spanish, French, German, Italian, Dutch, Greek, Russian, and Chinese.

---

## 🛠️ Building From Source (Windows x64 - Visual Studio 2022)

### Prerequisites
- **Visual Studio 2022** (Desktop development with C++)
- **Microsoft Windows SDK** (Windows 10 / 11)
- **Git** & **7-Zip**

### Build Steps
1. Clone this repository:
   ```bash
   git clone https://github.com/RFSVIEIRA/RetroFe_VLC.git
