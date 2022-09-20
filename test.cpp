#include <stdio.h>
#include <malloc.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

#include <sys/stat.h>

#include "source/libs/inih/INIReader/INIReader.h"
#include "source/libs/inih/ini.h"
#include "source/modules/dropbox.h"
#include "source/modules/citra.h"
#include "source/modules/time.h"

std::vector<std::string> recurse_dir(std::string basepath, std::string additionalpath=""){
    std::vector<std::string> paths;
    DIR *dir;
    struct dirent *ent;
    std::string path(basepath + additionalpath);
    std::cout << path << std::endl;
    if((dir = opendir(path.c_str())) != NULL){
        while((ent = readdir(dir)) != NULL){
            std::string readpath(path + "/" + ent->d_name);
            std::vector<std::string> recurse = recurse_dir(basepath, additionalpath + "/" + ent->d_name);
            paths.insert(paths.end(), recurse.begin(), recurse.end());
        }
    } else {
        if(additionalpath != "") paths.push_back(additionalpath);
        else printf("Folder %s not found\n", basepath.c_str());
    }
    closedir(dir);
    return paths;
}



int main(int argc, char** argv){
    std::string checkpointPath = "3ds-sdmc-dev/3ds/Checkpoint";
    INIReader reader("3ds-sdmc-dev/3ds/3DSync.ini");

    if(reader.ParseError() < 0){
        printf("Unable to parse ini\n");
	return 1;
    }

    std::string refreshToken = reader.Get("Dropbox", "RefreshToken", "");
    std::string clientId = reader.Get("Dropbox", "ClientId", "");
    std::string clientSecret = reader.Get("Dropbox", "ClientSecret", "");
    std::string dropboxToken = get_dropbox_access_token(clientId, clientSecret, refreshToken);

    if (dropboxToken == "") {
        std::cout << "Failed to receive Dropbox access token, exiting" << std::endl;
        return 1;
    }
    downloadCitraSaves(dropboxToken, checkpointPath);

    // Upload to Dropbox
    std::map<std::string, std::string> values = reader.GetValues();
    std::map<std::string, std::string> paths;
    for(auto value : values){
	if(value.first.rfind("paths=", 0) == 0){
	    std::pair<std::string, std::string> key = std::make_pair(value.second, value.first.substr(6));
	    paths[value.first.substr(6)] = value.second;
	}
    }
    Dropbox dropbox(dropboxToken);
    for (auto path : paths) {
        dropbox.syncDirs(path.first, path.second);
    }
}
