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

#include "Utils.h"
#include "../Database/Configuration.h"
#include "Log.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <locale>
#include <list>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <unordered_map>


// Platform - specific includes for directory creation
#ifdef _WIN32
#include <direct.h> // For _mkdir on Windows
#include< windows.h >
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h> // For mkdir on Unix-like systems
#define MKDIR(path) mkdir(path, 0755)
#endif

Utils::Utils()
{
}

Utils::~Utils()
{
}


std::string Utils::toLower(std::string str)
{
    for(unsigned int i=0; i < str.length(); ++i)
    {
        std::locale loc;
        str[i] = std::tolower(str[i], loc);
    }

    return str;
}

std::string Utils::uppercaseFirst(std::string str)
{
    if(str.length() > 0)
    {
        std::locale loc;
        str[0] = std::toupper(str[0], loc);
    }

    return str;
}
std::string Utils::filterComments(std::string line)
{
    size_t position;

    // strip out any comments
    if((position = line.find("#")) != std::string::npos)
    {
        line = line.substr(0, position);
    }
    // unix only wants \n. Windows uses \r\n. Strip off the \r for unix.
    line.erase( std::remove(line.begin(), line.end(), '\r'), line.end() );
    
    return line;
}

std::string Utils::combinePath(std::list<std::string> &paths)
{
    std::list<std::string>::iterator it = paths.begin();
    std::string path;

    if(it != paths.end())
    {
        path += *it;
        it++;
    }


    while(it != paths.end())
    {
        path += Utils::pathSeparator;
        path += *it;
        it++;
    }

    return path;
}

std::string Utils::combinePath(std::string path1, std::string path2)
{
    std::list<std::string> paths;
    paths.push_back(path1);
    paths.push_back(path2);
    return combinePath(paths);
}

std::string Utils::combinePath(std::string path1, std::string path2, std::string path3)
{
    std::list<std::string> paths;
    paths.push_back(path1);
    paths.push_back(path2);
    paths.push_back(path3);
    return combinePath(paths);
}

std::string Utils::combinePath(std::string path1, std::string path2, std::string path3, std::string path4)
{
    std::list<std::string> paths;
    paths.push_back(path1);
    paths.push_back(path2);
    paths.push_back(path3);
    paths.push_back(path4);
    return combinePath(paths);
}
std::string Utils::combinePath(std::string path1, std::string path2, std::string path3, std::string path4, std::string path5)
{
    std::list<std::string> paths;
    paths.push_back(path1);
    paths.push_back(path2);
    paths.push_back(path3);
    paths.push_back(path4);
    paths.push_back(path5);
    return combinePath(paths);
}
std::string Utils::combinePath(std::string path1, std::string path2, std::string path3, std::string path4, std::string path5, std::string path6)
{
    std::list<std::string> paths;
    paths.push_back(path1);
    paths.push_back(path2);
    paths.push_back(path3);
    paths.push_back(path4);
    paths.push_back(path5);
    paths.push_back(path6);
    return combinePath(paths);
}
std::string Utils::combinePath(std::string path1, std::string path2, std::string path3,
    std::string path4, std::string path5, std::string path6,
    std::string path7) {
    std::list<std::string> paths = { path1, path2, path3, path4, path5, path6, path7 };
    return combinePath(paths);
}

// Option Menu use
std::string Utils::normalizePath(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    std::string drive;
    std::string remainingPath = path; // Work with a mutable copy

    // Extract drive letter if it exists
    if (path.length() > 1 && path[1] == ':') {
        drive = path.substr(0, 2);    // e.g., "C:"
        remainingPath = path.substr(2); // Process the rest in a separate variable
    }

    // Parse the path components
    for (char c : remainingPath) {
        if (c == pathSeparator) {
            if (current == ".." && !parts.empty()) {
                parts.pop_back(); // Go up one directory
            }
            else if (!current.empty() && current != ".") {
                parts.push_back(current);
            }
            current.clear();
        }
        else {
            current += c;
        }
    }
    if (!current.empty()) {
        if (current == ".." && !parts.empty()) {
            parts.pop_back();
        }
        else if (current != ".") {
            parts.push_back(current);
        }
    }

    // Build the normalized path
    std::string result;
    for (const auto& part : parts) {
        if (!result.empty()) result += pathSeparator;
        result += part;
    }

    // Prepend drive letter if it was present
    if (!drive.empty()) {
        result = drive + std::string(1, pathSeparator) + result;
    }

    return result;
}

bool Utils::findMatchingFile(std::string prefix,std::vector<std::string> &extensions, std::string &file)
{
    for(unsigned int i = 0; i < extensions.size(); ++i)
    {
        std::string temp = prefix + "." + extensions[i];
        temp = Configuration::convertToAbsolutePath(Configuration::absolutePath, temp);

        std::ifstream f(temp.c_str());

        if (f.good())
        {
            file = temp;
            return true;
        }
    }

    return false;
}


std::string Utils::replace(
    std::string subject,
    const std::string& search,
    const std::string& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}


float Utils::convertFloat(std::string content)
{
    float retVal = 0;
    std::stringstream ss;
    ss << content;
    ss >> retVal;

    return retVal;
}

int Utils::convertInt(std::string content)
{
    int retVal = 0;
    std::stringstream ss;
    ss << content;
    ss >> retVal;

    return retVal;
}

void Utils::replaceSlashesWithUnderscores(std::string &content)
{
    std::replace(content.begin(), content.end(), '\\', '_');
    std::replace(content.begin(), content.end(), '/', '_');
}


std::string Utils::getDirectory(std::string filePath)
{

    std::string directory = filePath;

    const size_t last_slash_idx = filePath.rfind(pathSeparator);
    if (std::string::npos != last_slash_idx)
    {
        directory = filePath.substr(0, last_slash_idx);
    }

    return directory;
}

std::string Utils::getParentDirectory(std::string directory)
{
    size_t last_slash_idx = directory.find_last_of(pathSeparator);
    if(directory.length() - 1 == last_slash_idx)
    {
        directory = directory.erase(last_slash_idx, directory.length()-1);
        last_slash_idx = directory.find_last_of(pathSeparator);
    }

    if (std::string::npos != last_slash_idx)
    {
        directory = directory.erase(last_slash_idx, directory.length());
    }

    return directory;
}


std::string Utils::getFileName(std::string filePath)
{

    std::string filename = filePath;

    const size_t last_slash_idx = filePath.rfind(pathSeparator);
    if (std::string::npos != last_slash_idx)
    {
        filename = filePath.erase(0, last_slash_idx+1);
    }

    return filename;
}


std::string Utils::trimEnds(std::string str)
{
    // strip off any initial tabs or spaces
    size_t trimStart = str.find_first_not_of(" \t");

    if(trimStart != std::string::npos)
    {
        size_t trimEnd = str.find_last_not_of(" \t");

        str = str.substr(trimStart, trimEnd - trimStart + 1);
    }

    return str;
}


void Utils::listToVector( std::string str, std::vector<std::string> &vec, char delimiter = ',' )
{
    std::size_t current, previous = 0;
    current = str.find( delimiter );
    while (current != std::string::npos)
    {
        vec.push_back( Utils::trimEnds( str.substr( previous, current - previous ) ) );
        previous = current + 1;
        current  = str.find( delimiter, previous );
    }
    vec.push_back( Utils::trimEnds( str.substr( previous, current - previous ) ) );
}


int Utils::gcd( int a, int b )
{
    if (b == 0)
        return a;
    return gcd( b, a % b );
}




bool Utils::createDirectories(const std::string& path)
{
    std::string currentPath;
    std::stringstream ss(path);
    std::string dir;
    bool firstSegment = true;

    while (std::getline(ss, dir, Utils::pathSeparator))
    {
        if (dir.empty()) continue; // Skip empty segments

        // On Windows, skip the drive letter
        if (firstSegment && dir.length() <= 2 && dir.find(':') != std::string::npos)
        {
            currentPath = dir; // Just set it (e.g., "F:"), don’t try to create it
            firstSegment = false;
            continue;
        }

        currentPath = currentPath.empty() ? dir : currentPath + Utils::pathSeparator + dir;

        struct stat st;
        if (stat(currentPath.c_str(), &st) != 0) 
        {
#ifdef _WIN32
            if (mkdir(currentPath.c_str()) != 0)
#else
            if (mkdir(currentPath.c_str(), 0755) != 0)
#endif
            {
                Logger::write(Logger::ZONE_ERROR, "Utils", "Failed to create directory: " + currentPath);
                return false;
            }
            Logger::write(Logger::ZONE_INFO, "Utils", "Created directory: " + currentPath);
        }
        else if (!S_ISDIR(st.st_mode))
        {
            Logger::write(Logger::ZONE_ERROR, "Utils", "Path exists but is not a directory: " + currentPath);
            return false;
        }
        firstSegment = false;
    }
    return true;
}

std::vector<std::string> Utils::getFilesInDirectory(const std::string& path, const std::vector<std::string>& extensions)
{
    std::vector<std::string> files;

#ifdef _WIN32
    // Windows implementation
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile((path + "\\*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            // Skip directories
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::string fileName = findData.cFileName;
                size_t dotPos = fileName.find_last_of('.');
                if (dotPos != std::string::npos)
                {
                    std::string ext = fileName.substr(dotPos + 1);
                    std::string lowerExt = toLower(ext); 
                    for (const auto& e : extensions)
                    {
                        if (lowerExt == toLower(e))
                        {
                            files.push_back(combinePath(path, fileName)); 
                            break;
                        }
                    }
                }
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
  #else
    // Unix-like implementation
    DIR* dir = opendir(path.c_str());
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL))
        {
            std::string fileName = entry->d_name;
            std::string fullPath = combinePath(path, fileName); 
            struct stat st;
            
            if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            {
                size_t dotPos = fileName.find_last_of('.');
                if (dotPos != std::string::npos)
                {
                    std::string ext = fileName.substr(dotPos + 1);
                    std::string lowerExt = toLower(ext); 
                    for (const auto& e : extensions)
                    {
                        if (lowerExt == toLower(e))
                        {
                            files.push_back(fullPath);
                            break;
                        }
                    }
                }
            }
        }
        closedir(dir);
    }
 #endif
    return files;
}

std::vector<std::string> Utils::getDirectoriesInPath(const std::string& path)
{
    std::vector<std::string> directories;

#ifdef _WIN32
    // Windows implementation
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile((path + "\\*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            // Include only directories, exclude "." and ".."
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                std::string dirName = findData.cFileName;
                if (dirName != "." && dirName != "..")
                {
                    directories.push_back(dirName);
                }
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
#else
    // Unix-like implementation
    DIR* dir = opendir(path.c_str());
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string dirName = entry->d_name;
            std::string fullPath = combinePath(path, dirName);
            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                if (dirName != "." && dirName != "..")
                {
                    directories.push_back(dirName);
                }
            }
        }
        closedir(dir);
    }
#endif

    // Sort directories alphabetically
    std::sort(directories.begin(), directories.end());
    return directories;
}


std::vector<std::string> Utils::readGamesFromSubFile(const std::string& subFilePath)
{
    std::vector<std::string> games;
    std::ifstream file(subFilePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open sub-file: " << subFilePath << std::endl;
        return games;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (!line.empty()) {
            games.push_back(line);
        }
    }
    file.close();
    return games;
}

std::string Utils::getFileNameWithoutExtension(const std::string& filePath)
{
    std::string fileName = getFileName(filePath);
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos != std::string::npos) {
        return fileName.substr(0, dotPos);
    }
    return fileName;
}

static std::unordered_map<std::string, bool> fileExistsCache;

bool Utils::fileExists(const std::string& path) {
    auto it = fileExistsCache.find(path);
    if (it != fileExistsCache.end()) {
        return it->second;
    }
    std::ifstream file(path.c_str());
    bool exists = file.good();
    fileExistsCache[path] = exists;
    return exists;
}

void  Utils::clearFileExistsCache() {
    fileExistsCache.clear();
}



time_t Utils::getFileModificationTime(const std::string& filepath)
{
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        std::cerr << "Failed to get file attributes. Error: " << GetLastError() << std::endl;
        return 0; 
    }
    ULARGE_INTEGER ull;
    ull.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
    return  static_cast<time_t>(ull.QuadPart / 10000000ULL - 11644473600ULL);
}
std::vector<std::string> Utils::split(const std::string& str, char delimiter)
{
    std::vector<std::string> result;
    std::size_t previous = 0;
    std::size_t current = str.find(delimiter);
    while (current != std::string::npos)
    {
        result.push_back(trimEnds(str.substr(previous, current - previous)));
        previous = current + 1;
        current = str.find(delimiter, previous);
    }
    result.push_back(trimEnds(str.substr(previous)));
    return result;
}

time_t Utils::getMaxFileModTime(const std::string& dirPath) {
    std::string searchPath = dirPath + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    time_t maxModTime = 0;

    if (hFind == INVALID_HANDLE_VALUE) {
        Logger::write(Logger::ZONE_ERROR, "Utils", "Failed to open directory: " + dirPath + " Error: " + std::to_string(GetLastError()));
        return 0;
    }

    do {
        // Skip directories and "."/".."
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string filePath = Utils::combinePath(dirPath, findData.cFileName);
            time_t modTime = Utils::getFileModificationTime(filePath);
            if (modTime > maxModTime) {
                maxModTime = modTime;
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return maxModTime;
}
