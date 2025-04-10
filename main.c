#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>
#include <conio.h>
#include <stdbool.h>
#include <shlobj.h>

#define LOG_FILE "duplikati_log.txt"

typedef struct {
    char *path;
    BYTE hash[16];
} FileEntry;

FileEntry *files = NULL;
int file_count = 0;
int file_capacity = 0;

int compute_md5(const char *filename, BYTE *hash_out)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE buffer[8192];
    DWORD bytesRead;
    BOOL result = FALSE;

    FILE *file = fopen(filename, "rb");
    if(!file) return 0;

    if(!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) goto cleanup;
    if(!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) goto cleanup;

    while((bytesRead = fread(buffer, 1, sizeof(buffer), file)) != 0)
        if(!CryptHashData(hHash, buffer, bytesRead, 0)) goto cleanup;

    DWORD hashLen = 16;
    if(!CryptGetHashParam(hHash, HP_HASHVAL, hash_out, &hashLen, 0)) goto cleanup;

    result = TRUE;

cleanup:
    if(hHash) CryptDestroyHash(hHash);
    if(hProv) CryptReleaseContext(hProv, 0);
    fclose(file);
    return result;
}

void scan_directory(const char *base_path, BOOL recursive)
{
    char search_path[MAX_PATH];
    WIN32_FIND_DATAA findData;
    HANDLE hFind;

    snprintf(search_path, MAX_PATH, "%s\\*", base_path);
    hFind = FindFirstFileA(search_path, &findData);

    if(hFind == INVALID_HANDLE_VALUE) return;

    do {
        if(strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;

        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s\\%s", base_path, findData.cFileName);

        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if(recursive) scan_directory(full_path, recursive);
            else continue;
        }
        else
        {
            if(file_count >= file_capacity)
            {
                int new_capacity = file_capacity == 0 ? 1024 : file_capacity * 2;
                FileEntry *new_files = realloc(files, new_capacity * sizeof(FileEntry));
                if(!new_files)
                {
                    printf("Greska pri alokaciji memorije!\n");
                    return;
                }
                files = new_files;
                file_capacity = new_capacity;
            }
            files[file_count].path = _strdup(full_path);
            if(compute_md5(full_path, files[file_count].hash))
                file_count++;
            else
                free(files[file_count].path);
        }
    } while(FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

int hashes_equal(BYTE *hash1, BYTE *hash2)
{
    return memcmp(hash1, hash2, 16) == 0;
}

HWND hTitle;
HWND hTriTacke;
HWND hInfo;
HWND hPath;
HWND hPokreni;
HWND hGledajSF;
HFONT hTitleFont;
HFONT hFont;

char pathStr[MAX_PATH];

bool subfolderi = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int main(int argc, char **argv)
{
    CoInitialize(NULL);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "DuplikatClass";
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCE(101));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, "DuplikatClass", "Cistac Duplikata", WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, NULL, NULL, GetModuleHandleA(NULL), NULL);

    hTitle = CreateWindowExA(0, "STATIC", "Cistac Duplikata", WS_CHILD | WS_VISIBLE, 125, 10, 300, 30, hwnd, NULL, GetModuleHandleA(NULL), NULL);
    hInfo = CreateWindowExA(0, "STATIC", "Klikni '...' da odaberes folder, pa pritisni 'Pokreni'.", WS_CHILD | WS_VISIBLE, 20, 50, 375, 30, hwnd, NULL, GetModuleHandleA(NULL), NULL);
    hTriTacke = CreateWindowExA(0, "BUTTON", "...", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 80, 50, 30, hwnd, (HMENU)1, GetModuleHandleA(NULL), NULL);
    hPokreni = CreateWindowExA(0, "BUTTON", "Pokreni", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 250, 80, 100, 30, hwnd, (HMENU)2, GetModuleHandleA(NULL), NULL);
    hGledajSF = CreateWindowExA(0, "BUTTON", "Gledaj subfoldere", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 90, 80, 145, 30, hwnd, (HMENU)3, GetModuleHandleA(NULL), NULL);
    hPath = CreateWindowExA(0, "STATIC", "Odabran folder: nijedan", WS_CHILD | WS_VISIBLE, 20, 125, 375, 30, hwnd, (HMENU)4, GetModuleHandleA(NULL), NULL);

    hTitleFont = CreateFont(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));
    hFont = CreateFont(21, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));

    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
    SendMessage(hTriTacke, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPokreni, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hInfo, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hGledajSF, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPath, WM_SETFONT, (WPARAM)hFont, TRUE);

    if(hwnd == NULL)
    {
        MessageBoxA(NULL, "Prozor se nije pokrenuo", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);


    MSG msg = {0};
    while(GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    CoUninitialize();

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_COMMAND:
            if(LOWORD(wParam) == 1 && HIWORD(wParam) == BN_CLICKED)
            {
                BROWSEINFOA bi = {0};
                char szFolder[MAX_PATH];

                bi.lpszTitle = "Odaberi folder";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                bi.hwndOwner = hwnd;

                LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
                if(pidl != NULL)
                {
                    if(SHGetPathFromIDListA(pidl, szFolder))
                    {
                        strcpy(pathStr, szFolder);

                        char labelText[MAX_PATH + 32];
                        snprintf(labelText, sizeof(labelText), "Odabran folder: %s", (strlen(szFolder) > 0) ? szFolder : "nijedan");
                        SetWindowTextA(hPath, labelText);
                    }

                    CoTaskMemFree(pidl);
                }
            }
            if(LOWORD(wParam) == 2 && HIWORD(wParam) == BN_CLICKED)
            {
                if(strlen(pathStr) == 0) break;
                FILE *log = fopen(LOG_FILE, "w");
                if(!log)
                {
                    printf("Nemoguce otvoriti log file.\n");
                    return 1;
                }

                BOOL recursive = (IsDlgButtonChecked(hwnd, 3) == BST_CHECKED);

                files = NULL;
                file_count = 0;
                file_capacity = 0;

                scan_directory(pathStr, recursive);

                for(int i = 0; i < file_count; ++i)
                {
                    if(!files[i].path) continue;
                    for(int j = i + 1; j < file_count; ++j)
                    {
                        if(!files[j].path) continue;

                        if(hashes_equal(files[i].hash, files[j].hash))
                        {
                            if(DeleteFileA(files[j].path))
                                fprintf(log, "Izbrisan duplikat: %s (isti kao %s)\n", files[j].path, files[i].path);
                            else
                                fprintf(log, "Nemoguce obrisati: %s\n", files[j].path);
                            free(files[j].path);
                            files[j].path = NULL;
                        }
                    }
                }

                for(int i = 0; i < file_count; ++i)
                    free(files[i].path);

                free(files);

                fclose(log);
                MessageBoxA(hwnd, "Duplikati izbrisani. Pogledaj log file.", "Cistac Duplikata", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
