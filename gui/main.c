#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <shlobj.h>
#include "resource.h"

BOOL SelectFolderModern(HWND hwndOwner, char *outPath, size_t outSize) {
    IFileDialog *pfd = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IFileDialog, (void**)&pfd);
    if(FAILED(hr)) return FALSE;
    
    DWORD dwOptions;
    pfd->lpVtbl->GetOptions(pfd, &dwOptions);
    pfd->lpVtbl->SetOptions(pfd, dwOptions | FOS_PICKFOLDERS);
    
    hr = pfd->lpVtbl->Show(pfd, hwndOwner);
    if(SUCCEEDED(hr)) {
        IShellItem *psi;
        hr = pfd->lpVtbl->GetResult(pfd, &psi);
        if(SUCCEEDED(hr)) {
            PWSTR pszPath = NULL;
            hr = psi->lpVtbl->GetDisplayName(psi, SIGDN_FILESYSPATH, &pszPath);
            if(SUCCEEDED(hr)) {
                WideCharToMultiByte(CP_ACP, 0, pszPath, -1, outPath, outSize, NULL, NULL);
                CoTaskMemFree(pszPath);
                psi->lpVtbl->Release(psi);
                pfd->lpVtbl->Release(pfd);
                return TRUE;
            }
            psi->lpVtbl->Release(psi);
        }
    }
    pfd->lpVtbl->Release(pfd);
    return FALSE;
}

BOOL SelectFolder(HWND hwndOwner, char *outPath, size_t outSize) {
    OSVERSIONINFOA osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osvi);
    
    if(osvi.dwMajorVersion >= 6) {
        return SelectFolderModern(hwndOwner, outPath, outSize);
    }
    
    BROWSEINFOA bi = {0};
    bi.hwndOwner = hwndOwner;
    bi.lpszTitle = "Select a folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if(pidl && SHGetPathFromIDListA(pidl, outPath)) {
        CoTaskMemFree(pidl);
        return TRUE;
    }
    return FALSE;
}

#define JPG_MAGIC_0 0xFF
#define JPG_MAGIC_1 0xD8
#define JPG_MAGIC_2 0xFF

#pragma comment(lib, "comctl32.lib")

HWND hEditLog;

void append_log(const char *text) {
    int len = GetWindowTextLengthA(hEditLog);
    SendMessageA(hEditLog, EM_SETSEL, len, len);
    SendMessageA(hEditLog, EM_REPLACESEL, FALSE, (LPARAM)text);
}

void copy_file_time(const char *src, const char *dst) {
    HANDLE hSrc = CreateFileA(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hDst = CreateFileA(dst, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if(hSrc != INVALID_HANDLE_VALUE && hDst != INVALID_HANDLE_VALUE) {
        FILETIME ctime, atime, mtime;
        if(GetFileTime(hSrc, &ctime, &atime, &mtime)) {
            SetFileTime(hDst, &ctime, &atime, &mtime);
        }
    }

    if(hSrc != INVALID_HANDLE_VALUE) CloseHandle(hSrc);
    if(hDst != INVALID_HANDLE_VALUE) CloseHandle(hDst);
}

char *extract_file_time(const char *src) {
    static char buf[32];
    HANDLE hFile = CreateFileA(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        strcpy(buf, "unknown-CreateFileA");
        return buf;
    }

    FILETIME mtime, localTime;
    SYSTEMTIME stUTC, stLocal;

    if(GetFileTime(hFile, NULL, NULL, &mtime)) {
        FileTimeToLocalFileTime(&mtime, &localTime);
        FileTimeToSystemTime(&localTime, &stLocal);

        snprintf(buf, sizeof(buf), "%04d-%02d-%02d-%02d-%02d-%02d",
            stLocal.wYear,
            stLocal.wMonth,
            stLocal.wDay,
            stLocal.wHour,
            stLocal.wMinute,
            stLocal.wSecond);
    } else {
        strcpy(buf, "unknown-GetFileTime");
    }

    CloseHandle(hFile);
    return buf;
}

int ensure_dir(const char *path) {
    struct stat st = {0};
    if(stat(path, &st) == -1) {
        if(mkdir(path) != 0) {
            append_log("Failed to create output directory.\r\n");
            return 0;
        }
    }
    return 1;
}

char g_inputDir[MAX_PATH] = ".\\Photos";
char g_outputDir[MAX_PATH] = ".\\Converted";

void run_converter(BOOL useDate) {
    append_log("Starting conversion...\r\n");

    if(!ensure_dir(g_outputDir)) return;

    DIR *dir = opendir(g_inputDir);
    if(!dir) {
        append_log("Couldn't open Photos folder.\r\n");
        append_log("Create a 'Photos' folder next to this exe and put your screenshots there.\r\n");
        return;
    }

    struct dirent *entry;
    int count = 0;

    while((entry = readdir(dir)) != NULL) {
        if(strncmp(entry->d_name, "PRDR", 4) == 0 || strncmp(entry->d_name, "PGTA", 4) == 0) {
            char src_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", g_inputDir, entry->d_name);

            FILE *in = fopen(src_path, "rb");
            if(!in) continue;

            fseek(in, 0, SEEK_END);
            long size = ftell(in);
            fseek(in, 0, SEEK_SET);

            unsigned char *data = malloc(size);
            if(!data) {
                fclose(in);
                continue;
            }

            fread(data, 1, size, in);
            fclose(in);

            long pos = -1;
            for(long i = 0; i < size - 2; ++i) {
                if(data[i] == JPG_MAGIC_0 && data[i + 1] == JPG_MAGIC_1 && data[i + 2] == JPG_MAGIC_2) {
                    pos = i;
                    break;
                }
            }

            if(pos != -1) {
                char out_path[512];
                char *date = extract_file_time(src_path);
                if(!useDate) {
                    snprintf(out_path, sizeof(out_path), "%s/%s.jpg", g_outputDir, entry->d_name);
                } else {
                    snprintf(out_path, sizeof(out_path), "%s/%s.jpg", g_outputDir, date);
                }
                FILE *out = fopen(out_path, "wb");
                if(!out) {
                    free(data);
                    continue;
                }

                fwrite(data + pos, 1, size - pos, out);
                fclose(out);
                copy_file_time(src_path, out_path);

                char msg[512];
                snprintf(msg, sizeof(msg), "Converted: %s -> %s\r\n", entry->d_name, useDate ? date : entry->d_name);
                append_log(msg);
                count++;
            }

            free(data);
        }
    }

    closedir(dir);

    char done[128];
    snprintf(done, sizeof(done), "Finished converting %d file(s).\r\n", count);
    append_log(done);
}

HWND hLogo;
HBITMAP hBitmap;
HFONT hFont;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            LOGFONTA lf = {0};
            lf.lfHeight = -16;                    // ~12pt font
            lf.lfWeight = FW_NORMAL;
            strcpy(lf.lfFaceName, "Segoe UI");    // default modern Windows font
            hFont = CreateFontIndirectA(&lf);

            hBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_RDR2_BITMAP));
            if(!hBitmap) {
                append_log("ERROR: Failed to load bitmap!\r\n");
            } else {
                append_log("Bitmap loaded successfully!\r\n");
            }

            hLogo = CreateWindowA("STATIC", NULL,
                                  WS_VISIBLE | WS_CHILD | SS_BITMAP,
                                  390, 5, 75, 75,
                                  hwnd, NULL, GetModuleHandle(NULL), NULL);

            if(!hLogo) {
                append_log("ERROR: Failed to create static control!\r\n");
            }

            if(hBitmap) {
                LRESULT result = SendMessage(hLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
                if(result == 0) {
                    append_log("WARNING: STM_SETIMAGE returned 0\r\n");
                } else {
                    append_log("Bitmap set successfully!\r\n");
                }
            }

            HWND hBtnInput = CreateWindowA("BUTTON", "Select Photos Folder", WS_VISIBLE | WS_CHILD,
                          10, 10, 160, 25, hwnd, (HMENU)2, NULL, NULL);
            SendMessage(hBtnInput, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnOutput = CreateWindowA("BUTTON", "Select Output Folder", WS_VISIBLE | WS_CHILD,
                          180, 10, 160, 25, hwnd, (HMENU)3, NULL, NULL);
            SendMessage(hBtnOutput, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnConvert = CreateWindowA("BUTTON", "Convert", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                          10, 45, 100, 30, hwnd, (HMENU)1, NULL, NULL);
            SendMessage(hBtnConvert, WM_SETFONT, (WPARAM)hFont, TRUE);

            hEditLog = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                       WS_VISIBLE | WS_CHILD | WS_VSCROLL |
                                       ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                       10, 85, 460, 260, hwnd, NULL, NULL, NULL);
            SendMessage(hEditLog, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBoxDate = CreateWindowA("BUTTON", "Use date as filename", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 130, 45, 200, 30, hwnd, (HMENU)4, NULL, NULL);
            SendMessage(hBoxDate, WM_SETFONT, (WPARAM)hFont, TRUE);

            break;

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case 1:
                    SendMessageA(hEditLog, WM_SETTEXT, 0, (LPARAM)"");
                    BOOL useDate = (SendMessage(GetDlgItem(hwnd, 4), BM_GETCHECK, 0, 0) == BST_CHECKED);
                    run_converter(useDate);
                    break;
                case 2:
                    if(SelectFolder(hwnd, g_inputDir, sizeof(g_inputDir))) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Input folder: %s\r\n", g_inputDir);
                        append_log(msg);
                    }
                    break;
                case 3:
                    if(SelectFolder(hwnd, g_outputDir, sizeof(g_outputDir))) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Output folder: %s\r\n", g_outputDir);
                        append_log(msg);
                    }
                    break;
            }
            break;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
            return (LRESULT)GetStockObject(WHITE_BRUSH);

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSA wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "PhotoConvWin";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_RDR2_ICON));

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("PhotoConvWin", "RAGE Converter by NotStrahinja",
                              WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
                              NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    hBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_RDR2_BITMAP));

    UpdateWindow(hwnd);

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
