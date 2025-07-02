#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <shlwapi.h>
#include <shellapi.h> // Required for toast notifications

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib") // Required for Shell_NotifyIcon

// --- Global Constants ---
#define MUTEX_NAME "Global\\ScreenTimeRestrictorAppMutex"
#define IDLE_THRESHOLD_SECONDS 60
#define WM_TRAYICON (WM_USER + 1) // Custom message for tray icon events

// --- Global Variables for Settings (loaded from config.ini) ---
int TIME_LIMIT_SECONDS = 10800;
int WARNING_TIME_SECONDS = 300;
int GRACE_PERIOD_SECONDS_LOGIN = 600;

// --- Global Variables for File Paths ---
char usageFilePath[MAX_PATH];
char configFilePath[MAX_PATH];

// --- Global Variables for Application State ---
int used_seconds = 0;
int today_date = 0;
HWND hMainWnd = NULL;
NOTIFYICONDATA nid = {0}; // Structure for the notification icon

// --- Function Prototypes ---
void create_file_paths();
void load_config();
void write_default_config();
void load_usage();
void save_usage();
DWORD get_idle_time();
void ShowToastNotification(const char* title, const char* message);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

// --- Function Definitions ---

// Shows a toast-like balloon notification from the system tray.
void ShowToastNotification(const char* title, const char* message) {
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_WARNING; // Use a warning icon
    strncpy(nid.szInfoTitle, title, sizeof(nid.szInfoTitle) - 1);
    strncpy(nid.szInfo, message, sizeof(nid.szInfo) - 1);
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

// Constructs full paths for the usage and config files.
void create_file_paths() {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath);
    PathCombine(usageFilePath, exePath, "screen_usage.dat");
    PathCombine(configFilePath, exePath, "config.ini");
}

// Loads settings from config.ini or creates a default one.
void load_config() {
    if (!PathFileExists(configFilePath)) {
        write_default_config();
    }
    TIME_LIMIT_SECONDS = GetPrivateProfileInt("Settings", "TimeLimitSeconds", TIME_LIMIT_SECONDS, configFilePath);
    WARNING_TIME_SECONDS = GetPrivateProfileInt("Settings", "WarningTimeSeconds", WARNING_TIME_SECONDS, configFilePath);
    GRACE_PERIOD_SECONDS_LOGIN = GetPrivateProfileInt("Settings", "LoginGracePeriodSeconds", GRACE_PERIOD_SECONDS_LOGIN, configFilePath);
}

// Writes a default config.ini file if one doesn't exist.
void write_default_config() {
    WritePrivateProfileString("Settings", "TimeLimitSeconds", "TIME_LIMIT_SECONDS", configFilePath);
    WritePrivateProfileString("Settings", "WarningTimeSeconds", "WARNING_TIME_SECONDS", configFilePath);
    WritePrivateProfileString("Settings", "LoginGracePeriodSeconds", "GRACE_PERIOD_SECONDS_LOGIN", configFilePath);
    // This notification will be shown before the icon is ready, so it remains a MessageBox.
    MessageBox(NULL, "config.ini not found.\nA new one has been created with default settings.", "Configuration Created", MB_OK | MB_ICONINFORMATION);
}

// Loads today's usage from the data file.
void load_usage() {
    FILE* file = fopen(usageFilePath, "r");
    int stored_date = 0;
    if (file) {
        fscanf(file, "%d %d", &stored_date, &used_seconds);
        fclose(file);
    }
    if (stored_date != today_date) {
        used_seconds = 0;
    }
}

// Saves current usage to the data file.
void save_usage() {
    FILE* file = fopen(usageFilePath, "w");
    if (file) {
        fprintf(file, "%d %d\n", today_date, used_seconds);
        fclose(file);
    }
}

// Gets system idle time in seconds.
DWORD get_idle_time() {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    GetLastInputInfo(&lii);
    return (GetTickCount() - lii.dwTime) / 1000;
}

// Main logic callback, executed by the timer once per second.
VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (get_idle_time() < IDLE_THRESHOLD_SECONDS) {
        used_seconds++;
        int remaining_seconds = TIME_LIMIT_SECONDS - used_seconds;

        // --- WARNING LOGIC (Toast Notifications) ---
        if (remaining_seconds > 0 && remaining_seconds <= WARNING_TIME_SECONDS && remaining_seconds % 60 == 0) {
            char warningMsg[256];
            sprintf(warningMsg, "The system will lock in %d minute(s).", remaining_seconds / 60);
            ShowToastNotification("Screen Time Warning", warningMsg);
        }

        // --- SAVE LOGIC ---
        if (used_seconds % 60 == 0) {
            save_usage();
        }

        // --- LOCK LOGIC ---
        if (used_seconds >= TIME_LIMIT_SECONDS) {
            save_usage();
            // This is a critical, final notification, so it remains a blocking MessageBox.
            // MessageBox(NULL, "Screen time limit reached! Locking the system.", "Time Up", MB_OK | MB_ICONERROR | MB_TOPMOST);
            ShowToastNotification( "Time Up", "Screen time limit reached! Locking the system.");
            KillTimer(hMainWnd, 1);
            Sleep(2000);
            LockWorkStation();
            PostQuitMessage(0);
        }
    }
}

// Window procedure to handle messages for our hidden window.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Single instance check
    HANDLE hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Setup hidden window
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ScreenTimerHiddenWindow";
    RegisterClass(&wc);
    hMainWnd = CreateWindowEx(0, wc.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!hMainWnd) return 1;

    // --- SETUP NOTIFICATION ICON ---
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hMainWnd;
    nid.uID = 100;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_INFORMATION); // Use a standard system icon
    strcpy(nid.szTip, "Screen Time Restrictor");
    Shell_NotifyIcon(NIM_ADD, &nid);


    // Initialization
    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);
    today_date = (tm_info.tm_year + 1900) * 10000 + (tm_info.tm_mon + 1) * 100 + tm_info.tm_mday;
    create_file_paths();
    load_config(); // This might show a MessageBox if config is new, which is fine.
    load_usage();

    // Repeatable grace period logic
    if (used_seconds >= TIME_LIMIT_SECONDS) {
        used_seconds = TIME_LIMIT_SECONDS - GRACE_PERIOD_SECONDS_LOGIN;
        save_usage();
        ShowToastNotification("Grace Period", "Your screen time is up. A 10-minute grace period has been granted.");
    }
    
    // Timer setup
    SetTimer(hMainWnd, 1, 1000, TimerProc);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // --- CLEANUP ---
    Shell_NotifyIcon(NIM_DELETE, &nid); // Remove icon from tray on exit
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
