/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Launcher.h"
#include "../Collection/Item.h"
#include "../Utility/Log.h"
#include "../Database/Configuration.h"
#include "../Utility/Utils.h"
#include "../RetroFE.h"
#include "../SDL.h"
#include <cstdlib>
#include <locale>
#include <sstream>
#include <fstream>
#ifdef WIN32
#include <windows.h>
#include <shellapi.h>
#include <cstring>
#endif
#include <codecvt>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

Launcher::Launcher(Configuration& c, RetroFE* rfe)
    : config_(c)
    , retrofe_(rfe)
{
    
}

bool Launcher::run(std::string collection, Item *collectionItem)
{
    std::string launcherName = collectionItem->collectionInfo->launcher;
    std::string executablePath;
    std::string selectedItemsDirectory;
    std::string selectedItemsPath;
    std::string extensionstr;
    std::string matchedExtension;
    std::string args;

    std::string launcherFile = Utils::combinePath( Configuration::absolutePath, "collections", collectionItem->collectionInfo->name, "launchers", collectionItem->name + ".conf" );
    std::ifstream launcherStream( launcherFile.c_str( ) );
    if (launcherStream.good( )) // Launcher file found
    {
        std::string line;
        if (std::getline( launcherStream, line)) // Launcher found
        {
            launcherName = line;
        }
    }
    launcherName = Utils::toLower(launcherName);

    if(!launcherExecutable(executablePath, launcherName))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to find launcher executable (launcher: " + launcherName + " executable: " + executablePath + ")");
        return false;
    }
    if(!extensions(extensionstr, collection))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "No file extensions configured for collection \"" + collection + "\"");
        return false;
    }
    if(!collectionDirectory(selectedItemsDirectory, collection))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "Could not find files in directory \"" + selectedItemsDirectory + "\" for collection \"" + collection + "\"");
        return false;
    }
    if(!launcherArgs(args, launcherName))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "No launcher arguments specified for launcher " + launcherName);
        return false;
    }


    // Overwrite selectedItemsDirectory if already set in the file
    if (collectionItem->filepath != "")
    {
        selectedItemsDirectory = collectionItem->filepath;
    }

    // It is ok to continue if the file could not be found. We could be launching a merged romset
    if (collectionItem->file == "")
        findFile(selectedItemsPath, matchedExtension, selectedItemsDirectory, collectionItem->name, extensionstr);
    else
        findFile(selectedItemsPath, matchedExtension, selectedItemsDirectory, collectionItem->file, extensionstr);

    args = replaceVariables(args,
                            selectedItemsPath,
                            collectionItem->name,
                            Utils::getFileName(selectedItemsPath),
                            selectedItemsDirectory,
                            collection);

    executablePath = replaceVariables(executablePath,
                                      selectedItemsPath,
                                      collectionItem->name,
                                      Utils::getFileName(selectedItemsPath),
                                      selectedItemsDirectory,
                                      collection);

    std::string currentDirectoryKey = "launchers." + launcherName + ".currentDirectory";
    std::string currentDirectory    = Utils::getDirectory(executablePath);

    config_.getProperty(currentDirectoryKey, currentDirectory);

    currentDirectory = replaceVariables(currentDirectory,
                                        selectedItemsPath,
                                        collectionItem->name,
                                        Utils::getFileName(selectedItemsPath),
                                        selectedItemsDirectory,
                                        collection);

    if(!execute(executablePath, args, currentDirectory))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to launch.");
        return false;
    }

    bool reboot = false;
	config_.getProperty("launchers." + launcherName + ".reboot", reboot);
    lastRomPath_ = selectedItemsPath;
    return reboot;
    
}


void Launcher::LEDBlinky( int command, std::string collection, Item *collectionItem )
{
	std::string LEDBlinkyDirectory = "";
	config_.getProperty( "LEDBlinkyDirectory", LEDBlinkyDirectory );
	std::string exe  = LEDBlinkyDirectory + "\\LEDBlinky.exe";
	std::string args = std::to_string( command );
	bool wait = false;
	if ( command == 2 )
		wait = true;
	if ( command == 8 )
	{
		std::string launcherName = collectionItem->collectionInfo->launcher;
		std::string launcherFile = Utils::combinePath( Configuration::absolutePath, "collections", collectionItem->collectionInfo->name, "launchers", collectionItem->name + ".conf" );
		std::ifstream launcherStream( launcherFile.c_str( ) );
		if (launcherStream.good( )) // Launcher file found
		{
			std::string line;
			if (std::getline( launcherStream, line)) // Launcher found
			{
				launcherName = line;
			}
		}
		launcherName = Utils::toLower( launcherName );
		std::string emulator = collection;
		config_.getProperty("launchers." + launcherName + ".LEDBlinkyEmulator", emulator );
		args = args + " \"" + emulator + "\"";
	}
	if ( command == 3 || command == 9 )
	{
		std::string launcherName = collectionItem->collectionInfo->launcher;
		std::string launcherFile = Utils::combinePath( Configuration::absolutePath, "collections", collectionItem->collectionInfo->name, "launchers", collectionItem->name + ".conf" );
		std::ifstream launcherStream( launcherFile.c_str( ) );
		if (launcherStream.good( )) // Launcher file found
		{
			std::string line;
			if (std::getline( launcherStream, line)) // Launcher found
			{
				launcherName = line;
			}
		}
		launcherName = Utils::toLower( launcherName );
		std::string emulator = launcherName;
		config_.getProperty("launchers." + launcherName + ".LEDBlinkyEmulator", emulator );
		args = args + " \"" + collectionItem->name + "\" \"" + emulator + "\"";
		if ( emulator == "" )
			return;
	}
	if ( LEDBlinkyDirectory != "" && !execute( exe, args, LEDBlinkyDirectory, wait ) )
	{
        Logger::write( Logger::ZONE_ERROR, "LEDBlinky", "Failed to launch." );
	}
	return;
}


std::string Launcher::replaceVariables(std::string str,
                                       std::string itemFilePath,
                                       std::string itemName,
                                       std::string itemFilename,
                                       std::string itemDirectory,
                                       std::string itemCollectionName)
{
    str = Utils::replace(str, "%ITEM_FILEPATH%", itemFilePath);
    str = Utils::replace(str, "%ITEM_NAME%", itemName);
    str = Utils::replace(str, "%ITEM_FILENAME%", itemFilename);
    str = Utils::replace(str, "%ITEM_DIRECTORY%", itemDirectory);
    str = Utils::replace(str, "%ITEM_COLLECTION_NAME%", itemCollectionName);
    str = Utils::replace(str, "%RETROFE_PATH%", Configuration::absolutePath);


//#ifdef WIN32
//    str = Utils::replace(str, "%RETROFE_EXEC_PATH%", Utils::combinePath(Configuration::absolutePath, "core", "RetroFE.exe"));
#if _WIN32 || _WIN64
#if _WIN64
        str = Utils::replace(str, "%RETROFE_EXEC_PATH%", Utils::combinePath(Configuration::absolutePath, "corex64", "RetroFE.exe"));
#elif _WIN32
        str = Utils::replace(str, "%RETROFE_EXEC_PATH%", Utils::combinePath(Configuration::absolutePath, "core", "RetroFE.exe"));
#else
    str = Utils::replace(str, "%RETROFE_EXEC_PATH%", Utils::combinePath(Configuration::absolutePath, "RetroFE"));
#endif
#endif
    return str;
}

bool Launcher::execute(std::string executable, std::string args, std::string currentDirectory, bool wait)
{
    bool retVal = false;

    // Ensure relative executable and currentDirectory paths are resolved relative to RetroFE root
    if (!executable.empty())
    {
        char first = executable[0];
        char second = (executable.length() > 1) ? executable[1] : ' ';
        if (first != Utils::pathSeparator && second != ':')
        {
            std::string absCandidate = Utils::combinePath(Configuration::absolutePath, executable);
            struct stat buffer;
            if (stat(absCandidate.c_str(), &buffer) == 0)
            {
                executable = absCandidate;
            }
        }
    }

    if (!currentDirectory.empty())
    {
        char first = currentDirectory[0];
        char second = (currentDirectory.length() > 1) ? currentDirectory[1] : ' ';
        if (first != Utils::pathSeparator && second != ':')
        {
            std::string absDirCandidate = Utils::combinePath(Configuration::absolutePath, currentDirectory);
            struct stat buffer;
            if (stat(absDirCandidate.c_str(), &buffer) == 0)
            {
                currentDirectory = absDirCandidate;
            }
        }
    }

#ifdef WIN32
    // If cmd.exe is called with /C or /K and the path contains shell operators like &, protect quotes
    std::string exeLower = Utils::toLower(executable);
    if (exeLower == "cmd.exe" || exeLower == "cmd" ||
        (exeLower.length() >= 8 && (exeLower.compare(exeLower.length() - 8, 8, "\\cmd.exe") == 0 ||
                                    exeLower.compare(exeLower.length() - 8, 8, "/cmd.exe") == 0)))
    {
        if (args.length() >= 4 && (args[0] == '/' || args[0] == '-') &&
            (tolower(static_cast<unsigned char>(args[1])) == 'c' || tolower(static_cast<unsigned char>(args[1])) == 'k') &&
            args[2] == ' ' && args[3] == '\"')
        {
            if (args.find_first_of("&<>()@^|") != std::string::npos && !(args.length() >= 5 && args[4] == '\"'))
            {
                args = args.substr(0, 3) + "\"" + args.substr(3) + "\"";
            }
        }
    }

    SHELLEXECUTEINFOA sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpFile = executable.c_str();
    sei.lpParameters = args.empty() ? NULL : args.c_str();
    sei.lpDirectory = currentDirectory.empty() ? NULL : currentDirectory.c_str();
    sei.nShow = SW_SHOWDEFAULT;

    if (!ShellExecuteExA(&sei))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to run: " + executable);
    }
    else
    {
        if (sei.hProcess != NULL)
        {
            if (wait)
            {
                const DWORD TIMEOUT_MS = 16;  // ~60 FPS
                DWORD lastTime = GetTickCount();

                while (true)
                {
                    DWORD result = MsgWaitForMultipleObjects(1, &sei.hProcess, FALSE, TIMEOUT_MS, QS_ALLINPUT);

                    // Pump Windows messages
                    MSG msg;
                    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }

                    if (result == WAIT_OBJECT_0)
                    {
                        break;  // Game exited
                    }
                    else if (result == WAIT_TIMEOUT && retrofe_)
                    {
                        DWORD now = GetTickCount();
                        float delta = (now - lastTime) / 1000.0f;
                        lastTime = now;

                        retrofe_->updateSecondaryScreens(delta);
                        retrofe_->renderSecondaryScreens();
                    }
                }
            }

            CloseHandle(sei.hProcess);
        }
        retVal = true;
    }
#else
    const std::size_t last_slash_idx = executable.rfind(Utils::pathSeparator);
    if (last_slash_idx != std::string::npos)
    {
        std::string applicationName = executable.substr(last_slash_idx + 1);
        executionString = "cd \"" + currentDirectory + "\" && exec \"./" + applicationName + "\" " + args;
    }
    if (system(executionString.c_str()) != 0)
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to run: " + executable);
    }
    else
    {
        retVal = true;
    }
#endif

    Logger::write(Logger::ZONE_INFO, "Launcher", "Completed");

    return retVal;
}

bool Launcher::launcherName(std::string &launcherName, std::string collection)
{
    std::string launcherKey = "collections." + collection + ".launcher";

    // find the launcher for the particular item
    if(!config_.getProperty(launcherKey, launcherName))
    {
        std::stringstream ss;

        ss << "Launch failed. Could not find a configured launcher for collection \""
           << collection
           << "\" (could not find a property for \""
           << launcherKey
           << "\")";

        Logger::write(Logger::ZONE_ERROR, "Launcher", ss.str());

        return false;
    }

    std::stringstream ss;
    ss        << "collections."
              << collection
              << " is configured to use launchers."
              << launcherName
              << "\"";

    Logger::write(Logger::ZONE_DEBUG, "Launcher", ss.str());

    return true;
}



bool Launcher::launcherExecutable(std::string &executable, std::string launcherName)
{
    std::string executableKey = "launchers." + launcherName + ".executable";

    if(!config_.getProperty(executableKey, executable))
    {
        return false;
    }

    return true;
}

bool Launcher::launcherArgs(std::string &args, std::string launcherName)
{
    std::string argsKey = "launchers." + launcherName + ".arguments";

    if(!config_.getProperty(argsKey, args))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "No arguments specified for: " + argsKey);

        return false;
    }
    return true;
}

bool Launcher::extensions(std::string &extensions, std::string collection)
{
    std::string extensionsKey = "collections." + collection + ".list.extensions";

    if(!config_.getProperty(extensionsKey, extensions))
    {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "No extensions specified for: " + extensionsKey);
        return false;
    }

    extensions = Utils::replace(extensions, " ", "");
    extensions = Utils::replace(extensions, ".", "");

    return true;
}

bool Launcher::collectionDirectory(std::string &directory, std::string collection)
{
    std::string itemsPathValue;
    std::string mergedCollectionName;

    // find the items path folder (i.e. ROM path)
    config_.getCollectionAbsolutePath(collection, itemsPathValue);
    directory += itemsPathValue + Utils::pathSeparator;

    return true;
}

bool Launcher::findFile(std::string &foundFilePath, std::string &foundFilename, std::string directory, std::string filenameWithoutExtension, std::string extensions)
{
    std::string extension;
    bool fileFound = false;
    std::stringstream ss;
    ss << extensions;

    while(!fileFound && std::getline(ss, extension, ',') )
    {
        std::string selectedItemsPath = directory + filenameWithoutExtension + "." + extension;
        std::ifstream f(selectedItemsPath.c_str());

        if (f.good())
        {
            std::stringstream ss;

            ss        <<"Checking to see if \""
                      << selectedItemsPath << "\" exists  [Yes]";

            fileFound = true;

            Logger::write(Logger::ZONE_INFO, "Launcher", ss.str());

            foundFilePath = selectedItemsPath;
            foundFilename = extension;
        }
        else
        {
            std::stringstream ss;

            ss        << "Checking to see if \""
                      << selectedItemsPath << "\" exists  [No]";

            Logger::write(Logger::ZONE_WARNING, "Launcher", ss.str());
        }

        f.close();
    }

    // get the launchers executable

    if(!fileFound)
    {
        std::stringstream ss;
        ss        <<"Could not find any files with the name \""
                  << filenameWithoutExtension << "\" in folder \""
                  << directory;

        Logger::write(Logger::ZONE_WARNING, "Launcher", ss.str());

    }

    return fileFound;
}
bool Launcher::launchSearch() {
    std::string searchExe = Utils::combinePath(Configuration::absolutePath, "settings", "SEARCH", "SearchUI.exe");
    std::string searchDir = Utils::getDirectory(searchExe);

    if (!Utils::fileExists(searchExe)) {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "SearchUI.exe not found at: " + searchExe);
        return false;
    }

    // Launch fire-and-forget (no args, no wait)
    if (!execute(searchExe, "", searchDir, true)) {
        Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to launch SearchUI.exe");
        return false;
    }

    Logger::write(Logger::ZONE_INFO, "Launcher", "Launched SearchUI.exe successfully");
    return true;
}
void Launcher::ScreenDuplicator(Item* item, const std::string& romPath)
{
    std::string exe = Utils::combinePath(Configuration::absolutePath,
        "settings", "TOOLS", "Screen", "ScreenDuplicator.exe");
    if (!Utils::fileExists(exe))
    {
        Logger::write(Logger::ZONE_WARNING, "ScreenDuplicator", "EXE not found: " + exe);
        return;
    }

    std::string baseName = Utils::getFileNameWithoutExtension(item->name);
    std::string mirrorDir = Utils::combinePath(Configuration::absolutePath,
        "collections", item->collectionInfo->name,
        "mirror");
    std::string jsonPath = Utils::combinePath(mirrorDir, baseName + ".json");
    std::string jsonFolder = Utils::combinePath(mirrorDir, baseName);

    std::vector<std::string> jsonFiles;
    if (Utils::fileExists(jsonPath))
    {
        jsonFiles.push_back(jsonPath);
    }
    else if (fs::exists(jsonFolder) && fs::is_directory(jsonFolder))
    {
        for (const auto& entry : fs::directory_iterator(jsonFolder))
        {
            if (entry.is_regular_file())
            {
                std::string fname = entry.path().filename().string();
                if (fname == baseName + ".json")
                {
                    jsonFiles.push_back(entry.path().string());
                    continue;
                }
                if (fname.rfind(".json") == fname.length() - 5 && fname.find(baseName + "-") == 0)
                {
                    std::string suffix = fname.substr(baseName.length() + 1, fname.length() - baseName.length() - 1 - 5);
                    bool isNumber = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
                    if (isNumber)
                    {
                        jsonFiles.push_back(entry.path().string());
                    }
                }
            }
        }
        if (jsonFiles.empty())
        {
            Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "No valid JSONs found in folder: " + jsonFolder);
            return;
        }
        // Sort: main JSON first (if present), then others by numerical suffix
        auto mainIt = std::find_if(jsonFiles.begin(), jsonFiles.end(), [&](const std::string& f) {
            return fs::path(f).filename() == baseName + ".json";
            });
        std::vector<std::string> additional;
        std::string main;
        if (mainIt != jsonFiles.end())
        {
            main = *mainIt;
            for (auto& f : jsonFiles)
            {
                if (f != main) additional.push_back(f);
            }
        }
        else
        {
            additional = jsonFiles;
        }
        std::sort(additional.begin(), additional.end(), [&](const std::string& a, const std::string& b) {
            std::string sa = fs::path(a).stem().string().substr(baseName.length() + 1);
            std::string sb = fs::path(b).stem().string().substr(baseName.length() + 1);
            int ia = std::stoi(sa);
            int ib = std::stoi(sb);
            return ia < ib;
            });
        jsonFiles.clear();
        if (!main.empty()) jsonFiles.push_back(main);
        jsonFiles.insert(jsonFiles.end(), additional.begin(), additional.end());
    }
    else
    {
        Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "No JSON ? skip: " + jsonPath);
        return;
    }

    std::string dir = Utils::getDirectory(exe);
    for (const std::string& json : jsonFiles)
    {
        std::string args = "\"" + json + "\" --hide --launch";
        if (execute(exe, args, dir, false))
            Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "? LAUNCHED: " + item->name + " (" + fs::path(json).filename().string() + ")");
        else
            Logger::write(Logger::ZONE_ERROR, "ScreenDuplicator", "? FAILED to launch: " + item->name + " (" + fs::path(json).filename().string() + ")");
    }
}

// Modified with-handle function (changed return type to std::vector<HANDLE> to support multiple instances)
// You'll need to update the class member to std::vector<HANDLE> screenDuplicatorHandles;
// and adjust the run() code to: screenDuplicatorHandles = l.ScreenDuplicatorWithHandle(nextPageItem_, l.getLastRomPath());
// Later in your code (e.g., cleanup), loop over screenDuplicatorHandles to WaitForMultipleObjects, TerminateProcess, or CloseHandle as needed.
std::vector<HANDLE> Launcher::ScreenDuplicatorWithHandle(Item* item, const std::string& romPath)
{
    std::string exe = Utils::combinePath(Configuration::absolutePath,
        "settings", "TOOLS", "Screen", "ScreenDuplicator.exe");
    if (!Utils::fileExists(exe))
    {
        Logger::write(Logger::ZONE_WARNING, "ScreenDuplicator", "EXE not found: " + exe);
        return {};
    }

    std::string baseName = Utils::getFileNameWithoutExtension(item->name);
    std::string mirrorDir = Utils::combinePath(Configuration::absolutePath,
        "collections", item->collectionInfo->name,
        "mirror");
    std::string jsonPath = Utils::combinePath(mirrorDir, baseName + ".json");
    std::string jsonFolder = Utils::combinePath(mirrorDir, baseName);

    std::vector<std::string> jsonFiles;
    if (Utils::fileExists(jsonPath))
    {
        jsonFiles.push_back(jsonPath);
    }
    else if (fs::exists(jsonFolder) && fs::is_directory(jsonFolder))
    {
        for (const auto& entry : fs::directory_iterator(jsonFolder))
        {
            if (entry.is_regular_file())
            {
                std::string fname = entry.path().filename().string();
                if (fname == baseName + ".json")
                {
                    jsonFiles.push_back(entry.path().string());
                    continue;
                }
                if (fname.rfind(".json") == fname.length() - 5 && fname.find(baseName + "-") == 0)
                {
                    std::string suffix = fname.substr(baseName.length() + 1, fname.length() - baseName.length() - 1 - 5);
                    bool isNumber = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
                    if (isNumber)
                    {
                        jsonFiles.push_back(entry.path().string());
                    }
                }
            }
        }
        if (jsonFiles.empty())
        {
            Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "No valid JSONs found in folder: " + jsonFolder);
            return {};
        }
        // Sort: main JSON first (if present), then others by numerical suffix
        auto mainIt = std::find_if(jsonFiles.begin(), jsonFiles.end(), [&](const std::string& f) {
            return fs::path(f).filename() == baseName + ".json";
            });
        std::vector<std::string> additional;
        std::string main;
        if (mainIt != jsonFiles.end())
        {
            main = *mainIt;
            for (auto& f : jsonFiles)
            {
                if (f != main) additional.push_back(f);
            }
        }
        else
        {
            additional = jsonFiles;
        }
        std::sort(additional.begin(), additional.end(), [&](const std::string& a, const std::string& b) {
            std::string sa = fs::path(a).stem().string().substr(baseName.length() + 1);
            std::string sb = fs::path(b).stem().string().substr(baseName.length() + 1);
            int ia = std::stoi(sa);
            int ib = std::stoi(sb);
            return ia < ib;
            });
        jsonFiles.clear();
        if (!main.empty()) jsonFiles.push_back(main);
        jsonFiles.insert(jsonFiles.end(), additional.begin(), additional.end());
    }
    else
    {
        Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "No JSON ? skip: " + jsonPath);
        return {};
    }

    std::string dir = Utils::getDirectory(exe);
    std::vector<HANDLE> handles;
    for (const std::string& json : jsonFiles)
    {
        std::string args = "\"" + json + "\" --hide --launch";
        std::string cmdLine = "\"" + exe + "\" " + args;
        char* cmdMutable = _strdup(cmdLine.c_str());
        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        BOOL success = CreateProcess(
            NULL, cmdMutable, NULL, NULL,
            FALSE, 0, NULL, dir.c_str(),
            &si, &pi
        );
        free(cmdMutable);
        if (success)
        {
            CloseHandle(pi.hThread);
            Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "LAUNCHED WITH HANDLE: " + item->name + " (" + fs::path(json).filename().string() + ")");
            handles.push_back(pi.hProcess);
        }
        else
        {
            Logger::write(Logger::ZONE_ERROR, "ScreenDuplicator", "FAILED TO LAUNCH: " + item->name + " (" + fs::path(json).filename().string() + ")");
        }
    }
    return handles;
}

bool Launcher::launchRetroRa(const std::string& gameName, const std::string& systemName, bool forceRefresh)
{
    // No config check here anymore — caller already decided to call us
    std::string exe = Utils::combinePath(
        Configuration::absolutePath,
        "settings", "TOOLS", "RetroRa", "RetroRa.exe"
    );

    if (!Utils::fileExists(exe))
    {
        Logger::write(Logger::ZONE_ERROR, "RetroRa",
            "RetroRa.exe not found at: " + exe);
        return false;
    }

    // Escape quotes if present in game/system names
    std::string safeGame = Utils::replace(gameName, "\"", "\\\"");
    std::string safeSystem = Utils::replace(systemName, "\"", "\\\"");

    // Build base arguments
    std::string args = "-- \"" + safeGame + "\" -- \"" + safeSystem + "\"";

    // Append --refresh only when requested
    if (forceRefresh)
    {
        args += " --refresh";
    }

    std::string workingDir = Utils::getDirectory(exe);

    Logger::write(Logger::ZONE_INFO, "RetroRa",
        "Launching: \"" + exe + "\" " + args + " (wd: " + workingDir + ")");

    // Launch asynchronously (no wait)
    bool success = execute(exe, args, workingDir, false);

    if (success)
    {
        std::string msg = "Launch OK";
        if (forceRefresh)
        {
            msg += " (forced refresh)";
        }
        Logger::write(Logger::ZONE_INFO, "RetroRa", msg);
    }
    else
    {
        Logger::write(Logger::ZONE_ERROR, "RetroRa", "Launch failed (execute returned false)");
    }

    return success;
}
//// TODO : Re-enable Kiosk Mode launch if needed in the future
//bool Launcher::launchKiosk() {
//    std::string kioskExe = Utils::combinePath(Configuration::absolutePath, "settings", "TOOLS", "KioskMode", "SimpleKioskMode.exe");
//    std::string kioskDir = Utils::getDirectory(kioskExe);
//    if (!Utils::fileExists(kioskExe)) {
//        Logger::write(Logger::ZONE_ERROR, "Launcher", "SimpleKioskMode.exe not found at: " + kioskExe);
//        return false;
//    }
//    Logger::write(Logger::ZONE_INFO, "Launcher", "Attempting to launch: \"" + kioskExe + "\" from within folder: " + kioskDir);
//
//#ifdef WIN32
//    // Convert to wide string for ShellExecuteExW
//    std::wstring wKioskExe = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(kioskExe);
//    std::wstring wKioskDir = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(kioskDir);
//
//    // Use ShellExecuteExW to launch with runas and wait for UAC prompt
//    SHELLEXECUTEINFOW sei = { sizeof(sei) };
//    sei.lpVerb = L"runas";
//    sei.lpFile = wKioskExe.c_str();
//    sei.lpDirectory = wKioskDir.c_str();
//    sei.nShow = SW_SHOWNORMAL;
//    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS; // Wait for process, get handle
//    sei.hwnd = NULL;
//
//    if (!ShellExecuteExW(&sei)) {
//        DWORD error = GetLastError();
//        Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to launch SimpleKioskMode.exe: Error code " + std::to_string(error));
//        return false;
//    }
//
//    // Wait for the process to start or UAC prompt to be canceled
//    if (sei.hProcess) {
//        WaitForSingleObject(sei.hProcess, 5000); // Wait up to 5 seconds for process to start
//        CloseHandle(sei.hProcess);
//    }
//    Logger::write(Logger::ZONE_INFO, "Launcher", "Launched SimpleKioskMode.exe successfully with administrative privileges");
//    return true;
//#else
//    Logger::write(Logger::ZONE_WARNING, "Launcher", "Kiosk Mode not supported on non-Windows platforms");
//    return false;
//#endif
//}