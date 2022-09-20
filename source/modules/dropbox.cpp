#include "dropbox.h"
#include <json-c/json.h>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>
#include <string>
#include <iostream>
#include <fstream>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef REAL
#include <3ds.h>
#endif

const std::string CLIENT_ID = "teitrp9woa19r5o";
const std::string CLIENT_SECRET = "0dbq9sv2hd3nnc0";
//const std::string CLIENT_ID = "z4n5nrlgoypivuw";

Dropbox::Dropbox(std::string token) : _token(token){
}

const std::string timeFormat{"%Y-%m-%dT%H:%M:%SZ"};

int64_t getTimeStamp(const std::string& timeString)
{
    std::istringstream m_istream{timeString};
    std::tm m_tm { 0 };
    std::time_t m_timet { 0 };
    m_istream >> std::get_time(&m_tm, timeFormat.c_str());
    m_timet = std::mktime(&m_tm);
    //m_timet += 1 * 60 * 60;
    return m_timet;
}

std::string timestampString(std::time_t time)
{
    std::tm *ptm = std::localtime(&time);
    char buffer[32];
    std::strftime(buffer, 32, timeFormat.c_str(), ptm);
    return buffer;
}

std::string readFile(std::string path) {
    std::string line;
    std::string contents = "";
    std::ifstream file(path);
    if (file.is_open()) {
        while(getline(file, line)) {
            contents += line;
	}
	file.close();
    } else {
        return "";
    }
    return contents;
}


int writeFile(std::string path, std::string contents) {
    std::ofstream file(path);
    if (file.is_open()) {
	file << contents;
    } else {
	std::cout << "Failed to open " << path << " for writing.";
	return 1;
    }
    return 0;
}

std::string get_dropbox_access_token(std::string clientId, std::string clientSecret, std::string refreshToken) {
    if (false) {
        return refreshToken;
    }
    std::string postFields = "client_id=" + clientId + "&client_secret=" + clientSecret + "&grant_type=refresh_token&refresh_token=" + refreshToken;

    Curl _curl;
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Connection: close");
    _curl.setURL(std::string("https://api.dropboxapi.com/oauth2/token?" + postFields));
    //std::cout << std::string("https://api.dropboxapi.com/oauth2/token?" + postFields) << std::endl;
    _curl.setHeaders(headers);

    std::string httpData;
    _curl.setWriteData(&httpData);
    _curl.setCustomRequestPost();

    _curl.perform();

    auto http_code = _curl.getHTTPCode();

    if (http_code != 200) {
       std::cout << "Token auth error: Received " << http_code << std::endl;
       return "";
    }

    struct json_object *parsed_json;
    struct json_object *access_token;
    parsed_json = json_tokener_parse(httpData.c_str());
    json_object_object_get_ex(parsed_json, "access_token", &access_token);

    std::string access_token_str = json_object_get_string(access_token);
    return access_token_str;
}

void Dropbox::upload(std::map<std::pair<std::string, std::string>, std::vector<std::string>> paths){
    for(auto item : paths){
        for(auto path : item.second){
            printf("Uploading %s\n", (item.first.first + path).c_str());
            FILE *file = fopen((item.first.first + path).c_str(), "rb");
            std::string args("Dropbox-API-Arg: {\"path\":\"/" + item.first.second + path + "\",\"mode\": \"add\",\"mute\": false,\"strict_conflict\": false}");
            std::string auth("Authorization: Bearer " + _token);
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, auth.c_str());
            headers = curl_slist_append(headers, args.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
            headers = curl_slist_append(headers, "Expect:");
            _curl.setURL(std::string("https://content.dropboxapi.com/2/files/upload"));
            _curl.setHeaders(headers);
            _curl.setReadData((void *)file);
            _curl.perform();
            fclose(file);
            printf("\n");
        }
    }
}

std::vector<std::string> ls_dir(std::string basepath) {
    std::vector<std::string> paths;
    DIR *dir;
    struct dirent *ent;
    if((dir = opendir(basepath.c_str())) != NULL){
        while((ent = readdir(dir)) != NULL){
            //if (ent->d_type != DT_REG); continue;
	    // Skip special timestamp metadata files
	    std::string name = std::string(ent->d_name);
	    if (name.find("timestamp") != std::string::npos) {
                continue;
	    }
            std::string readpath(basepath + "/" + name);
	    std::string basefile = readpath.substr(readpath.find_last_of("/\\") + 1);
            if (basefile != "." && basefile != "..") {
                paths.insert(paths.end(), basefile);
	    }
        }
    } else {
        printf("Folder %s not found\n", basepath.c_str());
    }
    closedir(dir);
    return paths;
}

bool timezoneSafe(time_t bigger, time_t smaller)
{
    /**
    std::cout << "\nTIMEZONE CHECK" << std::endl;
    std::cout << "Bigger: " << bigger << " Smaller: " << smaller;
    std::cout << "Diff: " << bigger-smaller << std::endl;;
    std::cout << "Less than twelve hours: " << ((bigger - smaller) < 12 * 60 * 60) << std::endl; 
    std::cout << "Hour interval: " << ((bigger - smaller) % (60 * 60) == 0) << std::endl; 
    **/
    return (bigger - smaller < 12 * 60 * 60 && ((bigger - smaller) % (60 * 60) == 0));
}

void Dropbox::syncDirs(std::string cloudDir, std::string localDir){
    struct stat localFileInfo;
    std::uint64_t localFileModtime = 0;
    printf("\nSyncing %s with %s\n\n", cloudDir.c_str(), localDir.c_str());

    // Get the cached timestamps json file
    struct json_object *parsed_json = json_object_from_file((localDir + "/.sync_timestamps.json").c_str());
    if (parsed_json == NULL) {
        parsed_json = json_object_new_object();
    }
    struct json_object *localEntry;
    struct json_object *localCloudModTime;
    struct json_object *localCloudWriteTime;
    //std::string cache_contents = readFile("localDir" + "/" + ".sync_timestamps.json");
    //parsed_json = json_tokener_parse(cache_contents.c_str());

    auto results = list_folder(cloudDir, true);
    std::map<std::string, std::pair<int64_t, int64_t>> fileTimes;
    int count = 0;
    int cloudSize = results.size();
    for (auto lr : results) {
	    std::string cloudSave = cloudDir + "/" + lr.name;
	    std::string localSave = localDir + "/" + lr.name;

	    // Check for local save times with .timestamp files since we can't modify real mtimes
	    std::string localSaveTime = "";
#ifdef REAL
	    // Attempt to store local and cloud mod times
	    if (archive_getmtime(localSave.c_str(), &localFileModtime) == 0) {
                localSaveTime = timestampString(localFileModtime);
	    }
#endif
#ifndef REAL
	    if (stat(localSave.c_str(), &localFileInfo) == 0) {
		localSaveTime = timestampString(localFileInfo.st_mtime);
	    }
#endif
	    bool usingCache = false;
	    if (localSaveTime == "") {
                // There is no local file, safe to download
		fileTimes[lr.name] = std::make_pair(getTimeStamp(lr.client_modified) - 4 * 60 * 60, -1);
	    } else {
	        // Attempt to get timestapms from the cache	   
		if (parsed_json != NULL) {
                    json_object_object_get_ex(parsed_json, (lr.name).c_str(), &localEntry);
		    if (localEntry != NULL) {
		        if (json_object_object_get_ex(localEntry, "cloud", &localCloudModTime) && json_object_object_get_ex(localEntry, "write", &localCloudWriteTime)) {
                            if (localSaveTime == std::string(json_object_get_string(localCloudWriteTime))) {
                                // Use the cached cloud time since we wrote the file
		                fileTimes[lr.name] = std::make_pair(getTimeStamp(lr.client_modified) - 4 * 60 * 60, getTimeStamp(json_object_get_string(localCloudModTime)));
				usingCache = true;
			    }
			}
		    }
		}
	        if (!usingCache) {
                    // No cache, use real mtime from file
		    fileTimes[lr.name] = std::make_pair(getTimeStamp(lr.client_modified) - 4 * 60 * 60, getTimeStamp(localSaveTime));
		}
	    }

	    count++;
	    if (count % 10 == 0) {
                std::cout << "Analyzing cloud files " << count << "/" << cloudSize << std::endl;
	    }
    }
    std::vector<std::string> localContents = ls_dir(localDir);
    count = 0;
    int localSize = localContents.size();
    for (auto local : localContents) {
	if (fileTimes.count(local) == 0) {
            // Not yet in our map
#ifndef REAL
	    if (stat((localDir + "/" + local).c_str(), &localFileInfo) == 0) {
		fileTimes[local] = std::make_pair(-1, localFileInfo.st_mtime);
	    }
#endif
#ifdef REAL
	    if (archive_getmtime((localDir + "/" + local).c_str(), &localFileModtime)== 0) {
	        fileTimes[local] = std::make_pair(-1, localFileModtime);
	    }
#endif
	}

	count++;
	if (count % 10 == 0) {
	    std::cout << "Analyzing local files " << count << "/" << localSize << std::endl;
	}
    }
    count = 0;
    int fileCount = fileTimes.size();
    for (auto file : fileTimes) {
	//std::cout << file.first << ": Cloud time: " << timestampString(file.second.first) << " Local time: " << timestampString(file.second.second) << std::endl;
	time_t cloudTime = file.second.first;
	time_t localTime = file.second.second;
	if (cloudTime == -1) {
            std::cout << "[LOCAL][NEW] : " << file.first << " : New local save. Uploading. (local_mtime=" << timestampString(localTime) << ")" << std::endl;  
	    uploadFile(localDir + "/" + file.first, cloudDir + "/" + file.first, timestampString(localTime));
	}
	else if (localTime == -1) {
            std::cout << "[CLOUD][NEW] : " << file.first << " : New cloud save. Downloading. (cloud_mtime=" << timestampString(cloudTime) << ")" << std::endl;  
	    download(cloudDir + "/" + file.first, localDir, file.first, cloudTime, parsed_json);
	}
	else if (localTime < cloudTime && !timezoneSafe(cloudTime, localTime)) {
            std::cout << "[CLOUD] : " << file.first << " : Newer cloud save. Downloading. (cloud_mtime=" << timestampString(cloudTime) << " local_mtime=" + timestampString(localTime) + ")" << std::endl;  
	    download(cloudDir + "/" + file.first, localDir, file.first, cloudTime, parsed_json);
	}
	else if (localTime > cloudTime && !timezoneSafe(localTime, cloudTime)) {
            std::cout << "[LOCAL] : " << file.first << " : Newer local save. Uploading. (local_mtime=" << timestampString(localTime) << " cloud_mtime=" + timestampString(cloudTime) + ")" << std::endl;  
	    uploadFile(localDir + "/" + file.first, cloudDir + "/" + file.first, timestampString(localTime));
	}
	count++;
	if (count % 10 == 0) {
            std::cout << "Sync file " << count << "/" << fileCount << std::endl;
	}
    }

    json_object_to_file((localDir + "/.sync_timestamps.json").c_str(), parsed_json);
    /**
    FILE *file = fopen((item.first.first + path).c_str(), "rb");
    std::string args("Dropbox-API-Arg: {\"path\":\"/" + item.first.second + path + "\",\"mode\": \"add\",\"mute\": false,\"strict_conflict\": false}");
    std::string auth("Authorization: Bearer " + _token);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, args.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Expect:");
    _curl.setURL(std::string("https://content.dropboxapi.com/2/files/upload"));
    _curl.setHeaders(headers);
    _curl.setReadData((void *)file);
    _curl.perform();
    fclose(file);
    printf("\n");
    }
    **/
}

void Dropbox::deleteFile(std::string path) {
    std::string auth("Authorization: Bearer " + _token);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Connection: close");
    _curl.setURL(std::string("https://api.dropboxapi.com/2/files/delete_v2"));
    _curl.setHeaders(headers);
    std::string body("{\"path\":\"" + path + "\"}");
    _curl.setBody(body.c_str());

    std::string httpData;
    _curl.setWriteData(&httpData);
    _curl.perform();

#ifdef DEBUG
    std::cout << httpData << std::endl;
#endif

    auto http_code = _curl.getHTTPCode();
    if (http_code != 200) {
	if (http_code == 409) return;
        std::cout << "Delete error: " << http_code << std::endl;
    } else {
        std::cout << "Delete of " << path << " complete." << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    printf("\n");
}

void Dropbox::moveFile(std::string path) {
    std::string backUpPath = path + ".backup";

    printf("Moving %s to %s\n", path.c_str(), backUpPath.c_str());
    std::string args("Dropbox-API-Arg: {\"from_path\":\"/" + path + "\",\"to_path\":\"" + backUpPath + "\"");
    std::string auth("Authorization: Bearer " + _token);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, args.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Expect:");
    _curl.setURL(std::string("https://content.dropboxapi.com/2/files/move"));
    _curl.setHeaders(headers);
    _curl.perform();
    printf("\n");
}

void Dropbox::uploadFile(std::string path, std::string destPath, std::string modTime) {
    deleteFile(destPath);
    printf("Uploading\n%s\nto\n%s\nwith timestamp\n%s\n", path.c_str(), destPath.c_str(), modTime.c_str());

    struct stat info;

    if (stat(path.c_str(), &info) != 0) {
        printf("Error reading input file %s\n", path.c_str());
	return;
    }

    FILE *file = fopen(path.c_str(), "rb");
    std::string args("Dropbox-API-Arg: {\"path\":\"" + destPath + "\",\"mode\": \"overwrite\",\"mute\": false,\"strict_conflict\": false,\"client_modified\":\"" + modTime + "\"}");
    //std::string args("Dropbox-API-Arg: {\"path\":\"" + destPath + "\",\"mode\": \"overwrite\",\"mute\": false,\"strict_conflict\": false}");
    std::string auth("Authorization: Bearer " + _token);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, args.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Expect:");
    _curl.setURL(std::string("https://content.dropboxapi.com/2/files/upload"));
    _curl.setHeaders(headers);
    _curl.setReadData((void *)file);
    _curl.setPrintResult();
    _curl.perform();

    auto http_code = _curl.getHTTPCode();
    if (http_code != 200) {
        std::cout << "Upload error: " << http_code << std::endl;
    } else {
        std::cout << "Upload of " << path << " complete." << std::endl;
    }

    fclose(file);

    printf("\n");
}

std::vector<ListResult> Dropbox::list_folder(std::string path, bool only_files) {
    std::vector<ListResult> paths;

    std::string body("{\"path\":\"" + path + "\"}");
    std::string auth("Authorization: Bearer " + _token);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Connection: close");
    _curl.setURL(std::string("https://api.dropboxapi.com/2/files/list_folder"));
    _curl.setHeaders(headers);
    _curl.setBody(body.c_str());

    std::string httpData;
    _curl.setWriteData(&httpData);
    _curl.perform();

    auto http_code = _curl.getHTTPCode();

    if (http_code != 200) {
       std::cout << "List folder error: Received " << http_code << std::endl;
    } else {
        struct json_object *parsed_json;
        struct json_object *entries;
        parsed_json = json_tokener_parse(httpData.c_str());
        json_object_object_get_ex(parsed_json, "entries", &entries);

        auto entries_obj = json_object_get_array(entries);
        int len = array_list_length(entries_obj);
        for (int i = 0; i < len; i++) {
            struct json_object *name;
            struct json_object *path_display;
            struct json_object *server_modified;
            struct json_object *client_modified;
            std::string server_modified_str("");
            std::string client_modified_str("");
            struct json_object *tag;
            struct json_object *entry = (struct json_object *)(array_list_get_idx(entries_obj, i));
            json_object_object_get_ex(entry, "name", &name);
            json_object_object_get_ex(entry, "path_display", &path_display);
            json_object_object_get_ex(entry, ".tag", &tag);

            if (std::string(json_object_get_string(tag)) == "file") {
                json_object_object_get_ex(entry, "server_modified", &server_modified);
                server_modified_str = std::string(json_object_get_string(server_modified));
                json_object_object_get_ex(entry, "client_modified", &client_modified);
                client_modified_str = std::string(json_object_get_string(client_modified));

                ListResult lr = {
		    std::string(json_object_get_string(name)),
		    std::string(json_object_get_string(path_display)),
	            server_modified_str,
		    client_modified_str
		};

		paths.push_back(lr);
            } else if (!only_files) {
                ListResult lr = {
		    std::string(json_object_get_string(name)),
		    std::string(json_object_get_string(path_display)),
	            server_modified_str,
		    client_modified_str
		};

		paths.push_back(lr);
	    }

        }
    }

    return paths;
}

void Dropbox::download(std::string path, std::string destDir, std::string destFile, time_t cloudModTime, json_object* cache) {
    std::string destPath(destDir + "/" + destFile);
    std::cout << "Downloading " << path << " to " << destPath << std::endl;

    std::string args("Dropbox-API-Arg: {\"path\":\"" + path + "\"}");
    std::string auth("Authorization: Bearer " + _token);

    FILE *file = fopen((destPath).c_str(), "wb");
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, args.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Content-Length: 0");
    headers = curl_slist_append(headers, "Connection: close");
    _curl.setHeaders(headers);

    std::string headerData;
    _curl.setHeaderData(&headerData);

    _curl.setURL(std::string("https://content.dropboxapi.com/2/files/download"));
    _curl.setWriteFile(file);

    _curl.perform();

    auto http_code = _curl.getHTTPCode();

    fclose(file);

    if (http_code != 200) {
        std::cout << "Download error: Received " << http_code << std::endl;
    } else {
        // Write a timestamp file indicating when the file changed
	struct json_object *localEntry = json_object_new_object();
	struct json_object *cloudTime = json_object_new_string(timestampString(cloudModTime).c_str());
	json_object_object_add(localEntry, "cloud", cloudTime);
        // writeFile(destPath + ".cloud.timestamp", timestampString(cloudModTime));

        // Write a timestamp indicating when we wrote this file
        struct stat localFileInfo;
        std::uint64_t localFileModtime = 0;
    
	std::string writeTime = "";
#ifdef REAL
	// Attempt to store local and cloud mod times
	if (archive_getmtime(destPath.c_str(), &localFileModtime) != 0) {
            std::cout << "Unable to get write time for " << destPath;
	} else {
	    writeTime = timestampString(localFileModtime);
	}
#endif
#ifndef REAL
        if (stat((destPath).c_str(), &localFileInfo) != 0) {
            std::cout << "Unable to get write time for " << destPath;
        } else {
	    writeTime = timestampString(localFileInfo.st_mtime);
        }
#endif
        if (writeTime != "") {
	    // We need to track when the server wrote the file to know when it changed locally
	    struct json_object *jsonWriteTime = json_object_new_string(writeTime.c_str());
	    json_object_object_add(localEntry, "write", jsonWriteTime);
            //writeFile(destPath + ".write.timestamp", writeTime);
	}

	json_object_object_add(cache, destFile.c_str(), localEntry);
    }
}
