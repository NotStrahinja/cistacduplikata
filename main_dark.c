#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>
#include <conio.h>
#include <stdbool.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <vsstyle.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define MAX_EXTENSIONS 32
#define MAX_EXT_LEN 16

char extensions[MAX_EXTENSIONS][MAX_EXT_LEN];
int ext_count = 0;

void parse_extensions(const char* ext_string)
{
    ext_count = 0;

    char buffer[256];
    strncpy(buffer, ext_string, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = 0;

    char *token = strtok(buffer, ";");
    while(token != NULL && ext_count < MAX_EXTENSIONS)
    {
        if(token[0] == '.')
            token++;

        strncpy(extensions[ext_count], token, MAX_EXT_LEN - 1);
        extensions[ext_count][MAX_EXT_LEN - 1] = 0;

        for(char *p = extensions[ext_count]; *p; ++p)
            *p = (char)tolower(*p);

        ext_count++;
        token = strtok(NULL, ";");
    }
}

char exts[256] = "";

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

const char* get_file_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return NULL;

    return dot + 1;
}

void str_to_lower(char *str)
{
    while(*str)
    {
        *str = tolower((unsigned char)*str);
        str++;
    }
}

BOOL extension_allowed(const char *ext)
{
    if(!ext) return FALSE;

    char ext_lc[MAX_EXT_LEN];
    strncpy(ext_lc, ext, MAX_EXT_LEN - 1);
    ext_lc[MAX_EXT_LEN - 1] = '\0';
    str_to_lower(ext_lc);

    for(int i = 0; i < ext_count; i++)
    {
        if(strcmp(ext_lc, extensions[i]) == 0)
            return TRUE;
    }
    return FALSE;
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
HWND hKoristiEx;
HWND hEditExt;
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

    HMENU hMenu = CreateMenu();
    AppendMenuA(hMenu, MF_STRING, 4, "&Postavke");
    AppendMenuA(hMenu, MF_STRING, 5, "&Info");
    SetMenu(hwnd, hMenu);

    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

    hTitle = CreateWindowExA(0, "STATIC", "Cistac Duplikata", WS_CHILD | WS_VISIBLE, 125, 10, 300, 30, hwnd, NULL, GetModuleHandleA(NULL), NULL);
    hInfo = CreateWindowExA(0, "STATIC", "Klikni '...' da odaberes folder, pa pritisni 'Pokreni'.", WS_CHILD | WS_VISIBLE, 20, 50, 375, 30, hwnd, NULL, GetModuleHandleA(NULL), NULL);
    hTriTacke = CreateWindowExA(0, "BUTTON", "...", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 80, 50, 30, hwnd, (HMENU)1, GetModuleHandleA(NULL), NULL);
    hPokreni = CreateWindowExA(0, "BUTTON", "Pokreni", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 250, 80, 100, 30, hwnd, (HMENU)2, GetModuleHandleA(NULL), NULL);
    hPath = CreateWindowExA(0, "STATIC", "Odabran folder: nijedan", WS_CHILD | WS_VISIBLE, 20, 125, 375, 30, hwnd, (HMENU)4, GetModuleHandleA(NULL), NULL);

    hTitleFont = CreateFont(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));
    hFont = CreateFont(21, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));

    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
    SendMessage(hTriTacke, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPokreni, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hInfo, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hGledajSF, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPath, WM_SETFONT, (WPARAM)hFont, TRUE);

    SetWindowTheme(hPokreni, L"DarkMode_Explorer", NULL);
    SetWindowTheme(hTriTacke, L"DarkMode_Explorer", NULL);
    SetWindowTheme(hTitle, L"DarkMode_Explorer", NULL);
    SetWindowTheme(hInfo, L"DarkMode_Explorer", NULL);
    SetWindowTheme(hGledajSF, L"DarkMode_Explorer", NULL);
    SetWindowTheme(hPath, L"DarkMode_Explorer", NULL);

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

LRESULT OnCustomDraw(HWND hWnd, NMCUSTOMDRAW* pNMCD)
{
    LRESULT lResult = CDRF_DODEFAULT;
    switch (pNMCD->dwDrawStage)
    {
        case CDDS_PREPAINT:
        {
            SetBkMode(pNMCD->hdc, TRANSPARENT);
            SetTextColor(pNMCD->hdc, RGB(255, 255, 255));
        
            SIZE sizeCheckBox;
            HTHEME hTheme = OpenThemeData(NULL, TEXT(L"Button"));
            HRESULT hr = GetThemePartSize(hTheme, pNMCD->hdc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, NULL, TS_DRAW, &sizeCheckBox);
            CloseThemeData(hTheme);
            SIZE sizeExtent;
            GetTextExtentPoint32A(pNMCD->hdc, "0", 1, &sizeExtent);
            int nShift = sizeExtent.cx / 2;
                
            RECT rc = pNMCD->rc;
            OffsetRect(&rc, sizeCheckBox.cx + nShift, 0);
            char szText[MAX_PATH];
            GetWindowTextA(hWnd, szText, MAX_PATH);
            
            SelectObject(pNMCD->hdc, hFont);
            
            DrawTextA(pNMCD->hdc, szText, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_EXPANDTABS | DT_END_ELLIPSIS);
            lResult = CDRF_SKIPDEFAULT;
        }
        break;    
    }
    return lResult;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));
    HWND hCloseBut;
    HWND hSettingsWnd;
    static BOOL g_bGledajSubfoldere = FALSE;
    static BOOL useExtensions = FALSE;
    if(!strcmp(className, "DuplikatClass"))
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

                    BOOL recursive = g_bGledajSubfoldere;
                    BOOL ext = useExtensions;

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
                if(HIWORD(wParam) == 0)
                {
                    switch(LOWORD(wParam))
                    {
                        case 4:
                            WNDCLASSEXA wc = {0};
                            wc.cbSize = sizeof(WNDCLASSEXA);
                            wc.lpfnWndProc = WindowProc;
                            wc.hInstance = GetModuleHandle(NULL);
                            wc.lpszClassName = "SettingsClass";
                            wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
                            wc.hIcon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCE(101));
                            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                            HRESULT h = RegisterClassExA(&wc);
                            if(FAILED(h))
                            {
                                char errMsg[256];
                                sprintf(errMsg, "Error: %08x", h);
                                MessageBoxA(NULL, errMsg, "Error", MB_OK);
                            }
                            hSettingsWnd = CreateWindowExA(0, "SettingsClass", "Postavke", WS_CAPTION | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 300, 150, hwnd, NULL, GetModuleHandleA(NULL), NULL);
                            BOOL value = TRUE;
                            DwmSetWindowAttribute(hSettingsWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
                            hCloseBut = CreateWindowExA(0, "BUTTON", "Zatvori", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 225, 85, 60, 30, hSettingsWnd, (HMENU)6, GetModuleHandleA(NULL), NULL);
                            hGledajSF = CreateWindowExA(0, "BUTTON", "Gledaj subfoldere", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_NOTIFY, 10, 10, 145, 30, hSettingsWnd, (HMENU)3, GetModuleHandleA(NULL), NULL);
                            hKoristiEx = CreateWindowExA(0, "BUTTON", "Koristi ekstenzije", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_NOTIFY, 10, 40, 145, 30, hSettingsWnd, (HMENU)7, GetModuleHandleA(NULL), NULL);
                            hEditExt = CreateWindowExA(0, "EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 10, 85, 200, 30, hSettingsWnd, (HMENU)8, GetModuleHandleA(NULL), NULL);

                            SetWindowTheme(hCloseBut, L"DarkMode_Explorer", NULL);
                            SetWindowTheme(hGledajSF, L"DarkMode_Explorer", NULL);
                            SetWindowTheme(hKoristiEx, L"DarkMode_Explorer", NULL);
                            SetWindowTheme(hEditExt, L"DarkMode_Explorer", NULL);

                            SendMessage(hGledajSF, BM_SETCHECK, g_bGledajSubfoldere ? BST_CHECKED : BST_UNCHECKED, 0);
                            SendMessage(hKoristiEx, BM_SETCHECK, useExtensions ? BST_CHECKED : BST_UNCHECKED, 0);
                            SetWindowTextA(hEditExt, exts);
                            SendMessage(hCloseBut, WM_SETFONT, (WPARAM)hFont, TRUE);
                            SendMessage(hGledajSF, WM_SETFONT, (WPARAM)hFont, TRUE);
                            SendMessage(hKoristiEx, WM_SETFONT, (WPARAM)hFont, TRUE);
                            SendMessage(hEditExt, WM_SETFONT, (WPARAM)hFont, TRUE);
                            ShowWindow(hSettingsWnd, SW_SHOW);
                            UpdateWindow(hSettingsWnd);
                            break;
                        case 5:
                            MessageBoxA(NULL, "Napravio: Strahinja Adamov\nVerzija: 1.4", "Info", MB_OK | MB_ICONINFORMATION);
                            break;
                    }
                }
                break;
            case WM_CTLCOLORSTATIC:
                {
                    HDC hdc = (HDC)wParam;
                    static HBRUSH hDarkBrush = NULL;
                    if(!hDarkBrush)
                        hDarkBrush = CreateSolidBrush(RGB(38, 38, 38));
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkColor(hdc, RGB(38, 38, 38));
                    return (INT_PTR)hDarkBrush;
                }
                break;
            case WM_CTLCOLORBTN:
                {
                    HDC hdc = (HDC)wParam;
                    static HBRUSH hDarkBrush = NULL;
                    if(!hDarkBrush)
                        hDarkBrush = CreateSolidBrush(RGB(38, 38, 38));
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkColor(hdc, RGB(38, 38, 38));
                    return (INT_PTR)hDarkBrush;
                }
                break;
            case WM_ERASEBKGND:
                {
                    HDC hdc = (HDC)wParam;
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    FillRect(hdc, &rect, CreateSolidBrush(RGB(38, 38, 38)));
                    return 1;
                }
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            case WM_CLOSE:
                DestroyWindow(hwnd);
                break;
        }
    }
    else if(!strcmp(className, "SettingsClass"))
    {
        switch(uMsg)
        {
            case WM_NOTIFY:
                {
                    LRESULT lResult = 0;
                    switch(((NMHDR*)lParam)->code)
                    {
                        case NM_CUSTOMDRAW:
                            {
                                if(3 == ((NMHDR*)lParam)->idFrom || 7 == ((NMHDR*)lParam)->idFrom)
                                {
                                    NMHDR* pnm = (LPNMHDR)lParam;                
                                    lResult = OnCustomDraw(((NMHDR*)lParam)->hwndFrom, (LPNMCUSTOMDRAW)pnm);
                                }
                            }
                            break;
                        default:
                            return DefWindowProc(hwnd, uMsg, wParam, lParam);
                    }
                    return lResult;
                }
            case WM_COMMAND:
                if(LOWORD(wParam) == 3 && HIWORD(wParam) == BN_CLICKED)
                {
                    g_bGledajSubfoldere = (SendMessage(hGledajSF, BM_GETCHECK, 0, 0) == BST_CHECKED);
                }
                else if(LOWORD(wParam) == 7 && HIWORD(wParam) == BN_CLICKED)
                {
                    useExtensions = (SendMessage(hKoristiEx, BM_GETCHECK, 0, 0) == BST_CHECKED);
                }
                else if(LOWORD(wParam) == 6 && HIWORD(wParam) == BN_CLICKED)
                {
                    g_bGledajSubfoldere = (SendMessage(hGledajSF, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    useExtensions = (SendMessage(hKoristiEx, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    GetWindowTextA(hEditExt, exts, sizeof(exts));
                    DestroyWindow(hwnd);
                }
                break;
            case WM_CTLCOLORSTATIC:
                {
                    HDC hdc = (HDC)wParam;
                    static HBRUSH hDarkBrush = NULL;
                    if(!hDarkBrush)
                        hDarkBrush = CreateSolidBrush(RGB(38, 38, 38));
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkColor(hdc, RGB(38, 38, 38));
                    return (INT_PTR)hDarkBrush;
                }
                break;
            case WM_CTLCOLOREDIT:
                {
                    HDC hdc = (HDC)wParam;
                    static HBRUSH hDarkBrush = NULL;
                    if(!hDarkBrush)
                        hDarkBrush = CreateSolidBrush(RGB(38, 38, 38));
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkColor(hdc, RGB(38, 38, 38));
                    return (INT_PTR)hDarkBrush;
                }
                break;
            case WM_CTLCOLORBTN:
                {
                    HDC hdc = (HDC)wParam;
                    static HBRUSH hDarkBrush = NULL;
                    if(!hDarkBrush)
                        hDarkBrush = CreateSolidBrush(RGB(38, 38, 38));
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkColor(hdc, RGB(38, 38, 38));
                    return (INT_PTR)hDarkBrush;
                }
                break;
            case WM_ERASEBKGND:
                {
                    HDC hdc = (HDC)wParam;
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    FillRect(hdc, &rect, CreateSolidBrush(RGB(38, 38, 38)));
                    return 1;
                }
                break;
            case WM_DESTROY:
                DestroyWindow(hSettingsWnd);
                break;
            case WM_CLOSE:
                DestroyWindow(hSettingsWnd);
                break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
