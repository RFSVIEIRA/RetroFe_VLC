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

#include "Database/Configuration.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Execute/Launcher.h"
#include "Utility/Log.h"
#include "Utility/Utils.h"
#include "RetroFE.h"
#include "Version.h"
#include "SDL.h"
#include <cstdlib>
#include <fstream>
#include <dirent.h>
#include <time.h>
#include <locale>
#include <vlc/vlc.h>

static bool ImportConfiguration(Configuration* c);
static bool StartLogging();

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        std::string program = argv[0];
        std::string param = argv[1];

        if (argc == 3 && param == "-createcollection")
        {
            // Do nothing; we handle that later
        }
        else if (param == "-version" || param == "--version" || param == "-v")
        {
            std::string versionMsg = "RetroFE version " + Version::getString();
            std::cout << versionMsg << std::endl;
            Logger::write(Logger::ZONE_INFO, "RetroFE", versionMsg);
            return 0;
        }
        else
        {
            std::string usageMsg = "Usage:\n" +
                program + "                                           Run RetroFE\n" +
                program + " --version                                 Print the version of RetroFE.\n" +
                program + " -createcollection <collection name>       Create a collection directory structure.";
            std::cout << usageMsg << std::endl;
            Logger::write(Logger::ZONE_INFO, "RetroFE", usageMsg);
            return 0;
        }
    }

    setlocale(LC_ALL, "");
    srand(static_cast<unsigned int>(time(0)));
    Configuration::initialize();
    Configuration config;

    if (!StartLogging())
    {
        return -1;
    }

    if (argc == 3)
    {
        std::string param = argv[1];
        std::string value = argv[2];

        if (param == "-createcollection")
        {
            CollectionInfoBuilder::createCollectionDirectory(value);
        }

        return -1;
    }

    while (true)
    {
        if (!ImportConfiguration(&config))
        {
            std::string logFile = Utils::combinePath(Configuration::absolutePath, "log.txt");
            std::string errorMsg = "RetroFE has failed to start due to configuration error. Check log for details: " + logFile;
            fprintf(stderr, "%s\n", errorMsg.c_str());
            Logger::write(Logger::ZONE_ERROR, "RetroFE", errorMsg);
            return -1;
        }
        RetroFE p(config);
        if (p.run())
        {
            config.clearProperties();
            Logger::write(Logger::ZONE_INFO, "RetroFE", "Rebooting RetroFE instance");
        }
        else
        {
            break;
        }
    }

    Logger::deInitialize();
    return 0;
}

bool ImportConfiguration(Configuration* c)
{
    std::string configPath = Configuration::absolutePath;
#ifdef WIN32
    std::string launchersPath = Utils::combinePath(Configuration::absolutePath, "launchers.windows");
#elif __APPLE__
    std::string launchersPath = Utils::combinePath(Configuration::absolutePath, "launchers.apple");
#else
    std::string launchersPath = Utils::combinePath(Configuration::absolutePath, "launchers.linux");
#endif

    std::string collectionsPath = Utils::combinePath(Configuration::absolutePath, "collections");
    DIR* dp;
    struct dirent* dirp;

    std::string settingsConfPath = Utils::combinePath(configPath, "settings");
    c->import("", "", settingsConfPath + "_saved.conf", false);
    for (int i = 9; i > 0; i--)
        c->import("", "", settingsConfPath + std::to_string(i) + ".conf", false);
    if (!c->import("", settingsConfPath + ".conf"))
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not import \"" + settingsConfPath + ".conf\"");
        return false;
    }

    dp = opendir(launchersPath.c_str());

    if (dp == NULL)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Could not read directory \"" + launchersPath + "\"");
        launchersPath = Utils::combinePath(Configuration::absolutePath, "launchers");
        dp = opendir(launchersPath.c_str());
        if (dp == NULL)
        {
            Logger::write(Logger::ZONE_NOTICE, "RetroFE", "Could not read directory \"" + launchersPath + "\"");
            return false;
        }
    }

    while ((dirp = readdir(dp)) != NULL)
    {
        if (dirp->d_type != DT_DIR && std::string(dirp->d_name) != "." && std::string(dirp->d_name) != "..")
        {
            std::string basename = dirp->d_name;
            std::string::size_type dot_position = basename.find_last_of(".");

            if (dot_position == std::string::npos)
            {
                Logger::write(Logger::ZONE_NOTICE, "RetroFE", "Extension missing on launcher file \"" + basename + "\"");
                continue;
            }

            std::string extension = Utils::toLower(basename.substr(dot_position, basename.size() - 1));
            basename = basename.substr(0, dot_position);

            if (extension == ".conf")
            {
                std::string prefix = "launchers." + Utils::toLower(basename);

                std::string importFile = Utils::combinePath(launchersPath, std::string(dirp->d_name));

                if (!c->import(prefix, importFile))
                {
                    Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not import \"" + importFile + "\"");
                    if (dp) closedir(dp);
                    return false;
                }
            }
        }
    }

    if (dp) closedir(dp);

    dp = opendir(collectionsPath.c_str());

    if (dp == NULL)
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not read directory \"" + collectionsPath + "\"");
        return false;
    }

    while ((dirp = readdir(dp)) != NULL)
    {
        std::string collection = (dirp->d_name);
        if (dirp->d_type == DT_DIR && collection != "." && collection != ".." && collection.length() > 0 && collection[0] != '_')
        {
            std::string prefix = "collections." + collection;

            std::string infoFile = Utils::combinePath(collectionsPath, collection, "info.conf");

            c->import(collection, prefix, infoFile, false);

            std::string settingsFile = Utils::combinePath(collectionsPath, collection, "settings.conf");

            if (!c->import(collection, prefix, settingsFile, false))
            {
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Could not import \"" + settingsFile + "\"");
            }
        }
    }

    if (dp) closedir(dp);

    Logger::write(Logger::ZONE_INFO, "RetroFE", "Imported configuration");

    return true;
}

bool StartLogging()
{
    std::string logFile = Utils::combinePath(Configuration::absolutePath, "log.txt");

    if (!Logger::initialize(logFile))
    {
        std::string errorMsg = "Could not open log: " + logFile + " for writing! RetroFE will now exit...";
        fprintf(stderr, "%s\n", errorMsg.c_str());
        Logger::write(Logger::ZONE_ERROR, "RetroFE", errorMsg);
        std::ofstream fallbackLog("fallback_log.txt");
        if (fallbackLog.is_open())
        {
            fallbackLog << errorMsg << "\n";
            fallbackLog.close();
        }
        return false;
    }

    Logger::write(Logger::ZONE_INFO, "RetroFE", "Version " + Version::getString() + " starting");
#if _WIN32 || _WIN64
#if _WIN64
    Logger::write(Logger::ZONE_INFO, "RetroFE x64", "Version x64 " + Version::getString() + " starting");
#else
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Version x86 " + Version::getString() + " starting");
#endif
#endif

#ifdef WIN32
    Logger::write(Logger::ZONE_INFO, "RetroFE", "OS: Windows");
#elif __APPLE__
    Logger::write(Logger::ZONE_INFO, "RetroFE", "OS: Mac");
#else
    Logger::write(Logger::ZONE_INFO, "RetroFE", "OS: Linux");
#endif
    std::string versionVlc = libvlc_get_version();
    Logger::write(Logger::ZONE_INFO, "LibVlc", "Version " + versionVlc);
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Absolute path: " + Configuration::absolutePath);

    return true;
}