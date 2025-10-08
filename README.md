[TOC]

RetroFE is a cross-platform frontend designed for MAME cabinets, game centers, and more, with a focus on simplicity and customization. I’ve been working on enhancing this awesome project, originally created by phulshof, to make it even better for Windows users while staying true to its GPL3 license.
My Updates to RetroFE
I’m a curious enthusiast (not a formally trained C++ developer) who’s been tinkering with RetroFE to add new features and improve performance. Here’s what I’ve accomplished so far:

## My Objectives and Progress (Windows Only) ##
I’m focusing on Windows and have hit some challenges with Visual Studio, but here’s where I’m at:

- Menu Options: Added a new menu for settings, controls, and layout to make customization easier.
- 4K Support: Full 4K resolution support for stunning, high-definition visuals.
- Scale Quality Options: Adjust display quality for crisp clarity tailored to your setup.
- Rock-Solid Stability: Fixed issues so adding new screens doesn’t cause crashes.
- Animated Images: Menus now support GIF and WebP formats for a dynamic, lively interface.
- Memory Mastery: Addressed numerous memory leaks for smoother, more responsive performance.
- Playlist Power: New ordering options to personalize your playlists.
- Game Search: Added a search feature (via an external addon) to quickly find your favorite games.
- Dynamic Playlists: Introduced ALL GAMES, ALL FAVORITES, and ALL LASTPLAYED playlists with real-time updates (via an external addon).
- Layout Include Files: Simplified layout management for easier customization.
- Variables: Added support for variables to make layout design more flexible and dynamic.
- Grid: New menu option.

These changes are a labor of love, and I’m excited about the progress. My code might not be perfect—probably far from it—but it’s led to some significant improvements. I’m not here to claim I’m better than anyone; I’m just trying to contribute to the community.

### Call for Collaboration ###
I’d love for you to jump in with constructive criticism and contributions to make RetroFE even better. If there’s something to tear down, don’t worry—I’m already pretty good at being my own worst critic, haha! I have huge respect for everyone involved in RetroFE, especially phulshof, and I’m excited to collaborate with the community to keep improving this project.
If You Want Linux/Mac/Windows CMake Option
Download the original source code from phulshof’s repository:
git clone https://github.com/phulshof/RetroFE.git

If You Want My Windows Version (Visual Studio 2022)
This version is a migration/adaptation of the original RetroFE to Visual Studio 2022 standalone compiler, tailored for Windows users. I’ve made it user-friendly for Windows, but I can’t pull my changes to the original repository due to compatibility issues (or my lack of know-how). I’ve shared this GitHub link on the original RetroFE forum so the community can see it.
Source Code Changes

Modified SDL2 connections (e.g., changed #include <SDL2/SDL.h> to #include <SDL.h>).
Replaced GStreamer class with LibVLC class for media handling.
Replaced most libraries with NuGet packages for easier updates and automatic dependency management during compilation.

## Installing Required Libraries ##
To compile my version, you’ll need:

Visual Studio 2022
Microsoft Windows SDK for Windows 10 and .NET Framework 4
Git
7-Zip

Compiling and Installing on Windows

Open the RetroFE .sln file in Visual Studio 2022.
Choose Debug or Release mode.
Go to Build > Rebuild Solution.
The build will copy retrofe.exe and required DLLs to RetroFe/Corex64.
To clean the project, right-click the solution and select Clean.


## Final Notes ##
I’m deeply passionate about improving RetroFE for the community and am incredibly grateful for phulshof’s original work and the support of everyone involved. I’ve poured my heart into these updates, and I hope they add value to this amazing project. If phulshof finds these changes useful, I’d be honored for them to be considered for the original repository—it’s entirely in his hands. Thank you for exploring my work, and I’m excited to continue contributing to RetroFE’s journey!
