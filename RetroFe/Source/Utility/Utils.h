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
#pragma once

#include <string>
#include <vector>
#include <list>


class Utils
{
public:
    static std::string replace(std::string subject, const std::string& search,
                               const std::string& replace);
    int filetype;
    static float convertFloat(std::string content);
    static int convertInt(std::string content);
    static void replaceSlashesWithUnderscores(std::string &content);
    static std::string getDirectory(std::string filePath);
    static std::string getParentDirectory(std::string filePath);
    static std::string getFileName(std::string filePath);
    static bool createDirectories(const std::string& path);

    static bool findMatchingFile(std::string prefix, std::vector<std::string> &extensions, std::string &file);
    
    static std::string toLower(std::string str);
    static std::string uppercaseFirst(std::string str);
    static std::string filterComments(std::string line);
    static std::string trimEnds(std::string str);
    static void listToVector( std::string str, std::vector<std::string> &vec, char delimiter );
    static int gcd( int a, int b );

    //todo: there has to be a better way to do this
    static std::string combinePath(std::list<std::string> &paths);
    static std::string combinePath(std::string path1, std::string path2);
    static std::string combinePath(std::string path1, std::string path2, std::string path3);
    static std::string combinePath(std::string path1, std::string path2, std::string path3, std::string path4);
    static std::string combinePath(std::string path1, std::string path2, std::string path3, std::string path4, std::string path5);
    static std::string combinePath(std::string path1, std::string path2, std::string path3, std::string path4, std::string path5, std::string path6);
    static std::string combinePath(std::string path1, std::string path2, std::string path3, std::string path4, std::string path5, std::string path6, std::string path7);
   
    //Options Menu
    static std::string normalizePath(const std::string& path); 
    static std::vector<std::string> getFilesInDirectory(const std::string& path, const std::vector<std::string>& extensions);
    static std::vector<std::string> getDirectoriesInPath(const std::string& path);

    static std::string getFileNameWithoutExtension(const std::string& filePath);
    static std::vector<std::string> readGamesFromSubFile(const std::string& subFilePath);
    static bool fileExists(const std::string& path);

    void clearFileExistsCache();
 
    // Dynamic layout updating
    static time_t getFileModificationTime(const std::string& filepath);
    static time_t getMaxFileModTime(const std::string& dirPath);

    static std::vector<std::string> split(const std::string& str, char delimiter); 

#if defined (WIN32) || (_WIN64) 
    static const char pathSeparator = '\\';
#else
    static const char pathSeparator = '/';
#endif

private:
    Utils();
    virtual ~Utils();
};

