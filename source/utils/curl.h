#ifndef UTILS_CURL_H
#define UTILS_CURL_H

#include <string>

#include <curl/curl.h>

class Curl{
    public:
        Curl();
        ~Curl();
        void setURL(std::string URL);
        void setHeaders(curl_slist *headers);
        void setReadData(void *pointer);
        void setWriteData(void *pointer);
        void setPrintResult();
        void setHeaderData(void *pointer);
        void setWriteFile(FILE *pointer);
        void setBody(const void *pointer);
        void setCustomRequestPost();
        long getHTTPCode();
        int perform();
    private:
        CURL *_curl;
        static size_t _read_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
        static size_t _write_callback(void *data, size_t size, size_t nmemb, void *userdata);
        static size_t _header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
        static size_t _write_string_callback(const char* data, size_t size, size_t nmemb, std::string* userdata);
        static size_t _write_print_callback(const char* data, size_t size, size_t nmemb, std::string* userdata);
        static size_t _write_file_callback(void *ptr, size_t size, size_t nmemb, FILE *stream);
};


#endif
