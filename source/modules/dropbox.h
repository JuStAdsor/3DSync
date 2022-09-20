#ifndef MODULES_DROPBOX_H
#define MODULES_DROPBOX_H

#include <string>
#include <vector>
#include <json-c/json.h>
#include <map>

#include "../utils/curl.h"

typedef struct ListResult {
    std::string name;
    std::string path_display;
    std::string server_modified;
    std::string client_modified;
} ListResult;

std::string get_dropbox_access_token(std::string clientId, std::string clientSecret, std::string refreshToken);

class Dropbox{
    public:
        Dropbox(std::string token);
        ~Dropbox(){};
        void deleteFile(std::string path);
        void moveFile(std::string path);
        void upload(std::map<std::pair<std::string, std::string>, std::vector<std::string>> paths);
        void syncDirs(std::string cloudDir, std::string localDir);
        void uploadFile(std::string path, std::string destPath, std::string modTime);
        std::vector<ListResult> list_folder(std::string path, bool only_files);
        void download(std::string path, std::string destDir, std::string destFile, time_t cloudModtime, json_object* cache);
    private:
        std::string _token;
        Curl _curl;
};


#endif
