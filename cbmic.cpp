// --- CONFIGURATION ---
// Set your target mouse VID and PID here (as hex, uppercase, no 0x prefix)
#define UNICODE
#define _UNICODE
#define TARGET_VID L"1D57"
#define TARGET_PID L"AD05"
// --- END CONFIGURATION ---

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cctype>

// Remapping: button flag to virtual-key code
std::map<USHORT, WORD> remap;
// Track which keys are currently pressed down
std::map<WORD, bool> key_states;
std::wstring selected_mouse_id;
bool setup_done = false;

// Helper to build the device ID string
std::wstring get_target_id() {
    return std::wstring(L"VID_") + TARGET_VID + L"&PID_" + TARGET_PID;
}

bool is_target_mouse(const std::wstring& name) {
    return name.find(get_target_id()) != std::wstring::npos;
}

std::wstring get_device_name(HANDLE hDevice) {
    UINT size = 0;
    GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, nullptr, &size);
    std::wstring name(size, L'\0');
    if (size) {
        GetRawInputDeviceInfoW(hDevice, RIDI_DEVICENAME, &name[0], &size);
    }
    return name;
}

WORD ask_key(const std::wstring& btn_name) {
    std::wcout << L"Enter the keyboard key to map for " << btn_name << L" button (single letter, e.g. z): ";
    std::wstring input;
    std::getline(std::wcin, input);
    while (input.empty() || !std::isalpha(input[0])) {
        std::wcout << L"Invalid input. Please enter a single letter: ";
        std::getline(std::wcin, input);
    }
    return std::toupper(input[0]);
}

void send_key_down(WORD vk) {
    if (!key_states[vk]) {  // Only send if not already pressed
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        SendInput(1, &input, sizeof(INPUT));
        key_states[vk] = true;
    }
}

void send_key_up(WORD vk) {
    if (key_states[vk]) {  // Only send if currently pressed
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        key_states[vk] = false;
    }
}

void send_key(WORD vk) {
    INPUT input[2] = {};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = vk;
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = vk;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, input, sizeof(INPUT));
}

void cleanup_keys() {
    // Release all currently held keys
    for (auto& kv : key_states) {
        if (kv.second) {
            send_key_up(kv.first);
        }
    }
}

void print_remap_config() {
    std::wcout << L"Remapping config:" << std::endl;
    for (const auto& kv : remap) {
        std::wstring btn;
        if (kv.first == RI_MOUSE_LEFT_BUTTON_DOWN) btn = L"Left";
        else if (kv.first == RI_MOUSE_RIGHT_BUTTON_DOWN) btn = L"Right";
        else if (kv.first == RI_MOUSE_MIDDLE_BUTTON_DOWN) btn = L"Middle";
        else btn = L"Other";
        std::wcout << L"  " << btn << L" -> " << (wchar_t)kv.second << std::endl;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT) {
        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        std::vector<BYTE> lpb(dwSize);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
            return 0;

        RAWINPUT* raw = (RAWINPUT*)lpb.data();
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            std::wstring devName = get_device_name(raw->header.hDevice);
            if (!setup_done) {
                selected_mouse_id = devName;
                std::wcout << L"Selected mouse: " << devName << std::endl;
                remap[RI_MOUSE_LEFT_BUTTON_DOWN] = ask_key(L"Left");
                remap[RI_MOUSE_MIDDLE_BUTTON_DOWN] = ask_key(L"Middle");
                remap[RI_MOUSE_RIGHT_BUTTON_DOWN] = ask_key(L"Right");
                print_remap_config();
                std::wcout << L"Hold remapping started! Hold down a button on your mouse to hold the corresponding key. Press Ctrl+C to exit.\n" << std::endl;
                setup_done = true;
                return 0;
            }
            if (devName != selected_mouse_id) {
                return 0; // Ignore non-selected mice
            }
            
            // Handle button down events
            for (const auto& kv : remap) {
                if (raw->data.mouse.usButtonFlags & kv.first) {
                    std::wcout << L"Button pressed: ";
                    if (kv.first == RI_MOUSE_LEFT_BUTTON_DOWN) std::wcout << L"Left";
                    else if (kv.first == RI_MOUSE_RIGHT_BUTTON_DOWN) std::wcout << L"Right";
                    else if (kv.first == RI_MOUSE_MIDDLE_BUTTON_DOWN) std::wcout << L"Middle";
                    else std::wcout << L"Other";
                    std::wcout << L" -> Holding key: " << (wchar_t)kv.second << std::endl;
                    send_key_down(kv.second);
                }
            }
            
            // Handle button up events
            if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
                auto it = remap.find(RI_MOUSE_LEFT_BUTTON_DOWN);
                if (it != remap.end()) {
                    std::wcout << L"Button released: Left -> Releasing key: " << (wchar_t)it->second << std::endl;
                    send_key_up(it->second);
                }
            }
            if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
                auto it = remap.find(RI_MOUSE_RIGHT_BUTTON_DOWN);
                if (it != remap.end()) {
                    std::wcout << L"Button released: Right -> Releasing key: " << (wchar_t)it->second << std::endl;
                    send_key_up(it->second);
                }
            }
            if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
                auto it = remap.find(RI_MOUSE_MIDDLE_BUTTON_DOWN);
                if (it != remap.end()) {
                    std::wcout << L"Button released: Middle -> Releasing key: " << (wchar_t)it->second << std::endl;
                    send_key_up(it->second);
                }
            }
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL WINAPI ConsoleHandler(DWORD dwType) {
    switch (dwType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        cleanup_keys();
        exit(0);
        break;
    default:
        break;
    }
    return TRUE;
}

int main() {
    // Set up console handler for cleanup
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    
    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"RawInputWnd";
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, L"RawInput", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);

    // Register for raw input from all mice, block legacy input
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
    rid.hwndTarget = hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    std::wcout << L"Click any mouse button to select the mouse you want to remap..." << std::endl;

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    cleanup_keys();
    return 0;
}
// TODO: fix for better detection 
