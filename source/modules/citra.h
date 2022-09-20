#include <stdio.h>
#include <malloc.h>
#include <dirent.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

#include <sys/stat.h>

#include "dropbox.h"

const std::string timeFormat{"%Y-%m-%dT%H:%M:%SZ"};


std::string checkpointDirToCitraGameCode(std::string checkpointGameSaveDir) {
    if (checkpointGameSaveDir.rfind("0x", 0) == 0) {
        std::string gameCode = "0" + checkpointGameSaveDir.substr(2,5) + "00";
        for (auto &elem : gameCode) {
            elem = std::tolower(elem);
        }
        return gameCode;
    }
    return "";
}

std::map<std::string, std::string> findCheckpointSaves(std::string checkpointPath) {
    std::map<std::string, std::string> pathmap;

    std::string path(checkpointPath + "/saves");
    DIR *dir;
    struct dirent *ent;
    if((dir = opendir(path.c_str())) != NULL){
        while((ent = readdir(dir)) != NULL){
            std::string dirname = ent->d_name;

            if (dirname == "." || dirname == "..") {
                continue;
            }

            std::string readpath(path + "/" + dirname);

            std::string gameCode = checkpointDirToCitraGameCode(dirname);
            if (gameCode != "") {
                pathmap[gameCode] = dirname;
            } else {
                std::cout << "Invalid game directory, skipping: " << dirname << std::endl;
            }
        }
    } else {
        std::cout << "Failed to open checkpoint dir at " << path << std::endl;
    }

    return pathmap;
}

void downloadCitraSaveToCheckpoint(std::string dropboxToken, std::string checkpointPath, std::map<std::string, std::string> pathmap, std::string gameCode, std::string fullPath) {
    Dropbox dropbox(dropboxToken);

    std::string baseSaveDir = checkpointPath + "/saves";

    if (!pathmap.count(gameCode)) {
        //std::cout << "Game code " << gameCode << " not found in local Checkpoint saves, skipping" << std::endl;
        return;
    }

    std::string gameDirname = pathmap[gameCode];
    std::string gameSaveDir = baseSaveDir + "/" + gameDirname;
    std::string destPath = gameSaveDir + "/Citra";

    struct stat info;

    if (stat(destPath.c_str(), &info) != 0) {
        std::cout << "Creating dir: " << destPath << std::endl;
        int status = mkdir(destPath.c_str(), 0777);
        if (status != 0) {
            std::cout << "Failed to create Checkpoint save dir " << destPath << ", skipping" << std::endl;
            return;
        }
    } else if (info.st_mode & !S_IFDIR) {
        std::cout << "File already exists at Checkpoint save dir, delete the file and try again:\n" << destPath << std::endl;
    } else {
        //std::cout << "Checkpoint save dir already exists: " << destPath << std::endl;
    }

    dropbox.syncDirs(fullPath + "/data/00000001", destPath);
}

void downloadCitraSaves(std::string dropboxToken, std::string checkpointPath) {
    auto pathmap = findCheckpointSaves(checkpointPath);

    Dropbox dropbox(dropboxToken);
    auto folder = dropbox.list_folder("/saves/Citra/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000", false);
    //auto folder = dropbox.list_folder("/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000");
    for (auto lr : folder) {
        downloadCitraSaveToCheckpoint(
            dropboxToken,
            checkpointPath,
            pathmap,
            lr.name,
            lr.path_display
        );
    }
}
