#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define SRC_DIR "./Photos"
#define OUT_DIR "./Converted"
#define JPG_MAGIC_0 0xFF
#define JPG_MAGIC_1 0xD8
#define JPG_MAGIC_2 0xFF

#ifdef _WIN32
#include <windows.h>
void copy_file_time(const char *src, const char *dst) {
    HANDLE hSrc = CreateFileA(src,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    HANDLE hDst = CreateFileA(dst,
                              FILE_WRITE_ATTRIBUTES,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if(hSrc != INVALID_HANDLE_VALUE && hDst != INVALID_HANDLE_VALUE) {
        FILETIME ctime, atime, mtime;
        if(GetFileTime(hSrc, &ctime, &atime, &mtime)) {
            SetFileTime(hDst, &ctime, &atime, &mtime);
        }
    }

    if(hSrc != INVALID_HANDLE_VALUE) CloseHandle(hSrc);
    if(hDst != INVALID_HANDLE_VALUE) CloseHandle(hDst);
}
#endif

int ensure_dir(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path
#ifndef _WIN32
            , 0777
#endif
        ) != 0) {
            perror("mkdir");
            return 0;
        }
    }
    return 1;
}

int main() {
    printf("RAGEConverter by NotStrahinja\n");

    if(!ensure_dir(OUT_DIR)) {
        return 1;
    }

    DIR *dir = opendir(SRC_DIR);
    if(!dir) {
        perror("opendir");
        printf("\nPlease create a \"Photos\" folder and put your screenshots there in order to convert them.\n");
        printf("All of the instructions on how to find the screenshots are on my GitHub.");
        printf("\n\n\nPress ENTER to continue...");
        getchar();
        return 1;
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(strncmp(entry->d_name, "PRDR", 4) == 0 || strncmp(entry->d_name, "PGTA", 4) == 0) {
            char src_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", SRC_DIR, entry->d_name);

            FILE *in = fopen(src_path, "rb");
            if(!in) {
                perror("fopen (input)");
                continue;
            }

            fseek(in, 0, SEEK_END);
            long size = ftell(in);
            fseek(in, 0, SEEK_SET);

            unsigned char *data = malloc(size);
            if(!data) {
                fprintf(stderr, "Memory alloc failed\n");
                fclose(in);
                continue;
            }
            
            fread(data, 1, size, in);
            fclose(in);

            long pos = -1;
            for(long i = 0; i < size - 2; ++i) {
                if(data[i] == JPG_MAGIC_0 && data[i+1] == JPG_MAGIC_1 && data[i+2] == JPG_MAGIC_2) {
                    pos = i;
                    break;
                }
            }

            if(pos != -1) {
                char out_path[512];
                snprintf(out_path, sizeof(out_path), "%s/%s.jpg", OUT_DIR, entry->d_name);

                FILE *out = fopen(out_path, "wb");
                if(!out) {
                    perror("fopen (output)");
                    free(data);
                    continue;
                }

                fwrite(data + pos, 1, size - pos, out);
                fclose(out);
                printf("Converted %s -> %s\n", entry->d_name, out_path);
                #ifdef _WIN32
                copy_file_time(src_path, out_path);
                #endif
            } else {
                printf("%s: No JPG header", entry->d_name);
            }
            free(data);
        }
    }
    printf("Finished converting.\n");
    return 0;
}


