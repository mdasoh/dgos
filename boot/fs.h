#pragma once
#include "types.h"

typedef struct fs_api_t {
    int (*boot_open)(char const *filename);
    int (*boot_close)(int file);
    int (*boot_pread)(int file, void *buf, size_t bytes, off_t ofs);
} fs_api_t;

extern fs_api_t fs_api;

// Open/close/read file on the boot drive
int boot_open(char const *filename);
int boot_close(int file);
int boot_pread(int file, void *buf, size_t bytes, off_t ofs);