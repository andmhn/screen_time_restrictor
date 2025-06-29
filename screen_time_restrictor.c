#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <shlwapi.h> // Required for PathRemoveFileSpec and PathAppend. Link with -lshlwapi

#pragma comment(lib, "Shlwapi.lib") // For MSVC, tells the linker to include Shlwapi.lib
#pragma comment(lib, "User32.lib") // For MessageBox and LockWorkStation

// Default values if config file doesn't exist or values are invalid
#define DEFAULT_TIME_LIMIT 3600   // 1 hour in seconds
#define DEFAULT_WARNING_TIME 300  // 5 minutes before limit
#define DEFAULT_IDLE_THRESHOLD 60 // 60 seconds of inactivity
#define DEFAULT_GRACE_PERIOD 600  // 10-minute grace period

#define USAGE_FILE_NAME "screen_usage.dat"
#define CONFIG_FILE_NAME "config.ini"

// Global variables
char usageFilePath[MAX_PATH];
char configFilePath[MAX_PATH];
int TIME_LIMIT = DEFAULT_TIME_LIMIT;
int WARNING_TIME = DEFAULT_WARNING_TIME;
int IDLE_THRESHOLD = DEFAULT_IDLE_THRESHOLD;
int GRACE_PERIOD = DEFAULT_GRACE_PERIOD;

// --- FUNCTION PROTOTYPES ---
int get_today_date();
void create_file_paths();
int load_usage(int* stored_date, int* stored_time);
void save_usage(int date, int time_used);
DWORD get_idle_time();
void load_configuration();
void create_default_config();

// Gets the current date as an integer (e.g., 20240521)
int get_today_date() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    return tm.tm_year * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
}

// Constructs the full paths for both usage and config files
void create_file_paths() {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH); // Get path of the .exe
    PathRemoveFileSpec(exePath);               // Remove the .exe filename, leaving the directory
    
    // Create usage file path
    PathAppend(usageFilePath, exePath);        
    PathAppend(usageFilePath, USAGE_FILE_NAME);
    
    // Create config file path
    PathAppend(configFilePath, exePath);
    PathAppend(configFilePath, CONFIG_FILE_NAME);
}

// Creates a default config file if it doesn't exist
void create_default_config() {
    if (GetFileAttributes(configFilePath) == INVALID_FILE_ATTRIBUTES) {
        FILE* file = fopen(configFilePath, "w");
        if (file) {
            fprintf(file, "# Screen Time Restrictor Configuration\n");
            fprintf(file, "# All values are in seconds\n\n");
            fprintf(file, "TIME_LIMIT=%d       # 1 hour\n", DEFAULT_TIME_LIMIT);
            fprintf(file, "WARNING_TIME=%d     # 5 minutes before limit\n", DEFAULT_WARNING_TIME);
            fprintf(file, "IDLE_THRESHOLD=%d   # 1 minute of inactivity\n", DEFAULT_IDLE_THRESHOLD);
            fprintf(file, "GRACE_PERIOD=%d     # 10-minute grace period after exhausted user logs in.\n", DEFAULT_GRACE_PERIOD);
            fclose(file);
        }
    }
}

// Loads configuration from config.ini file
void load_configuration() {
    // First create default config if it doesn't exist
    create_default_config();
    
    FILE* file = fopen(configFilePath, "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            // Skip comments and empty lines
            if (line[0] == '#' || line[0] == '\n') continue;
            
            // Parse key-value pairs
            char key[64], value[64];
            if (sscanf(line, "%63[^=]=%63[^\n]", key, value) == 2) {
                if (strcmp(key, "TIME_LIMIT") == 0) TIME_LIMIT = atoi(value);
                else if (strcmp(key, "WARNING_TIME") == 0) WARNING_TIME = atoi(value);
                else if (strcmp(key, "IDLE_THRESHOLD") == 0) IDLE_THRESHOLD = atoi(value);
                else if (strcmp(key, "GRACE_PERIOD") == 0) GRACE_PERIOD = atoi(value);
            }
        }
        fclose(file);
    }
    
    // Validate loaded values
    if (TIME_LIMIT <= 0) TIME_LIMIT = DEFAULT_TIME_LIMIT;
    if (WARNING_TIME <= 0 || WARNING_TIME >= TIME_LIMIT) WARNING_TIME = DEFAULT_WARNING_TIME;
    if (IDLE_THRESHOLD <= 0) IDLE_THRESHOLD = DEFAULT_IDLE_THRESHOLD;
    if (GRACE_PERIOD <= 0 || GRACE_PERIOD >= TIME_LIMIT) GRACE_PERIOD = DEFAULT_GRACE_PERIOD;
}

// [Rest of the functions remain exactly the same as in previous implementation]
// Loads the total used time from the data file.
// If the file doesn't exist or the date has changed, it resets the time.
int load_usage(int* stored_date, int* stored_time) {
    FILE* file = fopen(usageFilePath, "r");
    if (file) {
        fscanf(file, "%d %d", stored_date, stored_time);
        fclose(file);
        return 1;
    }
    *stored_date = get_today_date();
    *stored_time = 0;
    return 0;
}

// Saves the current usage time and date to the data file.
void save_usage(int date, int time_used) {
    FILE* file = fopen(usageFilePath, "w");
    if (file) {
        fprintf(file, "%d %d\n", date, time_used);
        fclose(file);
    }
}

// Gets the system's idle time in seconds.
DWORD get_idle_time() {
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    GetLastInputInfo(&lii);
    return (GetTickCount() - lii.dwTime) / 1000;
}

// --- MAIN FUNCTION ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // Create a unique mutex to ensure single instance
    HANDLE hMutex = CreateMutex(NULL, TRUE, "Global\\ScreenTimeRestrictorMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, "Another instance is already running.", "Screen Time Restrictor", MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0; // Exit the application
    }

    
    int saved_date, used_seconds;
    int current_date = get_today_date();
    
    // Determine the paths for both files before using them
    create_file_paths();
    
    // Load configuration from file (will create default if needed)
    load_configuration();
    
    load_usage(&saved_date, &used_seconds);

    // If it's a new day, reset the timer.
    if (saved_date != current_date) {
        used_seconds = 0;
        saved_date = current_date;
        save_usage(saved_date, used_seconds); // Save the reset
    }

    // Grace Period Logic
    // If the user logs in and time is already up, grant a 10-minute grace period.
    if (used_seconds >= TIME_LIMIT) {
        char graceMsg[256];
        sprintf(graceMsg, "Your screen time for today is up.\nYou have a %d-minute grace period for essential tasks.", GRACE_PERIOD / 60);
        MessageBox(NULL, graceMsg, "Grace Period Granted", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        used_seconds = TIME_LIMIT - GRACE_PERIOD;
        save_usage(current_date, used_seconds);
    }

    while (1) {
        Sleep(1000); // Check every second

        if (get_idle_time() < IDLE_THRESHOLD) {
            used_seconds++;

            int remaining_seconds = TIME_LIMIT - used_seconds;

            // Logic for minute-by-minute warnings
            if (remaining_seconds > 0 && remaining_seconds <= WARNING_TIME) {
                if (remaining_seconds % 60 == 0) {
                    char warningMsg[256];
                    int minutes_left = remaining_seconds / 60;
                    sprintf(warningMsg, "⚠️ You are nearing your screen time limit!\n\nThe system will lock in %d minute(s).", minutes_left);
                    
                    MessageBox(NULL, warningMsg, "Screen Time Warning", MB_OK | MB_ICONWARNING | MB_TOPMOST);
                }
            }

            // Save usage every minute
            if (used_seconds > 0 && used_seconds % 60 == 0) {
                save_usage(current_date, used_seconds);
            }

            // Lock the workstation if the time limit is reached.
            if (used_seconds >= TIME_LIMIT) {
                MessageBox(NULL, "⛔ Screen time limit reached! Locking the system now.", "Time Up", MB_OK | MB_ICONERROR | MB_TOPMOST);
                save_usage(current_date, used_seconds);
                LockWorkStation();
                break; // Exit the loop
            }
        }
    }

    // Release the mutex when done (though this may not be reached if LockWorkStation is called)
    CloseHandle(hMutex);

    return 0;
}