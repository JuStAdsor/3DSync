#!/bin/bash -e

mkdir -p 3ds-sdmc-dev
g++ -o test test.cpp source/modules/dropbox.cpp source/utils/curl.cpp source/libs/inih/INIReader/INIReader.cpp source/libs/inih/ini.c -lcurl -ljson-c
./test
