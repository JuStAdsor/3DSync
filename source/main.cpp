#include <stdio.h>
#include <malloc.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <iostream>

#include <3ds.h>

#include <curl/curl.h>

#include "libs/inih/INIReader/INIReader.h"
#include "modules/dropbox.h"
#include "modules/citra.h"
#include "modules/time.h"

std::vector<std::string> recurse_dir(std::string basepath, std::string additionalpath=""){
    std::vector<std::string> paths;
    DIR *dir;
    struct dirent *ent;
    std::string path(basepath + additionalpath);
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

bool componentsInit(){
    bool result = true;
    gfxInitDefault();

    consoleInit(GFX_BOTTOM, NULL);
    printf(CONSOLE_RED "\n 3DSync " VERSION_STRING " by Kyraminol" CONSOLE_RESET);
    printf("\n\n\n\n\n\n  Sync your saves with another 3DS,\n   a PC or even a cloud.");
    printf("\n\n\n\n\n\n Commit: " CONSOLE_BLUE REVISION_STRING CONSOLE_RESET);

    consoleInit(GFX_TOP, NULL);
    printf("Initializing components...\n\n");
    
    APT_SetAppCpuTimeLimit(30);
    cfguInit();
    romfsInit();
    pxiDevInit();
    amInit();
    acInit();

    u32* socketBuffer = (u32*)memalign(0x1000, 0x100000);
    if(socketBuffer == NULL){printf("Failed to create socket buffer.\n"); result = false;}
    if(socInit(socketBuffer, 0x100000)){printf("socInit failed.\n"); result = false;}

    httpcInit(0);
    sslcInit(0);
    return result;
}

void componentsExit(){
    sslcExit();
    httpcExit();
    socExit();
    acExit();
    pxiDevExit();
    romfsExit();
    cfguExit();
    gfxExit();
}

void syncCheckPointSaves(std::string dropboxToken) {
    // Sync Citra saves to local Checkpoint directory and vice versa
    std::string checkpointPath = "/3ds/Checkpoint";
    if (checkpointPath != "") {
	downloadCitraSaves(dropboxToken, checkpointPath);
	std::cout << "Finished syncing Citra saves!" << std::endl;
    }
}

void syncCustomDirectories(std::string dropboxToken, INIReader reader) {
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

int main(int argc, char** argv){
    if(!componentsInit()) componentsExit();

    INIReader reader("/3ds/3DSync/3DSync.ini");

    if(reader.ParseError() < 0){
        printf("Can't load configuration\n");
    } else {
        std::string refreshToken = reader.Get("Dropbox", "RefreshToken", "");
        std::string clientId = reader.Get("Dropbox", "ClientId", "");
        std::string clientSecret = reader.Get("Dropbox", "ClientSecret", "");
        
        if(refreshToken != ""){

            std::string dropboxToken = get_dropbox_access_token(clientId, clientSecret, refreshToken);

            if (dropboxToken == "") {
                std::cout << "Failed to receive Dropbox access token, exiting" << std::endl;
                return 1;
            }

	    printf("\n    A: Sync Citra Saves\n    X: Sync Custom Directories\nSTART: Sync All\n    B: Quit Without Syncing\n");
	    while (aptMainLoop()){
		hidScanInput();
		u32 kDown = hidKeysDown();
		if (kDown & KEY_A) {
                    syncCheckPointSaves(dropboxToken);
		    break;
	        } else if (kDown & KEY_X) { 
                    syncCustomDirectories(dropboxToken, reader);
		    break;
		} else if (kDown & KEY_START) {
                    syncCheckPointSaves(dropboxToken);
                    syncCustomDirectories(dropboxToken, reader);
		    break;
		} else if (kDown & KEY_B) {
                    componentsExit();
		    return 0;
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	    }
        } else {
            printf("Can't load Dropbox token from 3DSync.ini\n");
        }

    }

    printf("\n\nPress START to exit...");
    while (aptMainLoop()){
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    componentsExit();
    return 0;
}
