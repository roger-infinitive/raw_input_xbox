#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <tchar.h>
#include <hidusage.h>
#include <hidsdi.h>

#define u16 unsigned short
#define u32 unsigned int

//TODO(roger): Replace with Arena and free arena at the end of loop
#define FrameAlloc(size) malloc(size)

char ASSERT_MESSAGE[2048];
char ASSERT_BOX_MESSAGE[2048];

enum XboxButtonCodes {
    Xbox_ButtonA     =  0,
    Xbox_ButtonB     =  1,
    Xbox_ButtonX     =  2,
    Xbox_ButtonY     =  3,
    Xbox_LeftBumper  =  4,
    Xbox_RightBumper =  5,
    Xbox_Start       =  6,
    Xbox_Select      =  7,
    Xbox_LeftStick   =  8,
    Xbox_RightStick  =  9,
    Xbox_Home        = 10,
};
    
// TODO(roger): Hardcoded size.
#define BUTTON_COUNT 16
bool buttonStates[16];

void ShowErrorMessageBox(const char* message) {
    int response = MessageBox(NULL, message, "Assertion Failure", MB_ICONERROR | MB_OKCANCEL);
    switch (response) {
        case IDOK: { __debugbreak(); } break;
    } 
    TerminateProcess(GetCurrentProcess(), 1);
}

#define Assert(condition, message, ...) \
    do { \
        if (!(condition)) { \
            sprintf(ASSERT_MESSAGE, message, __VA_ARGS__); \
            sprintf(ASSERT_BOX_MESSAGE, "ERROR: %s\n\tAssertion failed: %s\n\tFile: %s, Line: %d\n", \
                ASSERT_MESSAGE, #condition, __FILE__, __LINE__); \
            ShowErrorMessageBox(ASSERT_BOX_MESSAGE); \
        } \
    } while (0)

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INPUT: {
            // Get the size of the raw input to allocate buffer
            u32 rawInputSize;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, 0, &rawInputSize, sizeof(RAWINPUTHEADER));
            PRAWINPUT rawInput = (PRAWINPUT)FrameAlloc(rawInputSize);
    
            // GetRawInputData converts the handle from WM_INPUT into a RAWINPUT pointer.
            // The RAWINPUT struct tells us the device handle, type of input, and the input data for any device except for Mouse and Keyboard.
    
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawInput, &rawInputSize, sizeof(RAWINPUTHEADER));
            RAWHID *device = &rawInput->data.hid;
            
            u32 deviceInfoSize;
            u32 success = GetRawInputDeviceInfo(rawInput->header.hDevice, RIDI_PREPARSEDDATA, 0, &deviceInfoSize);
            Assert(success == 0, "Failed to get device info size.");
            
            PHIDP_PREPARSED_DATA preparsedData = (PHIDP_PREPARSED_DATA)FrameAlloc(deviceInfoSize);
            u32 bytesCopied = GetRawInputDeviceInfo(rawInput->header.hDevice, RIDI_PREPARSEDDATA, preparsedData, &deviceInfoSize);
            Assert(bytesCopied >= 0, "Failed to copy preparsedData into buffer.");
            
            // The device data we get from WM_INPUT message is a series of HID reports from the HIDClass driver, and we use 
            // the preparsed data to unpack HID reports and determine the pressed buttons and axis values.
            
            // Now we determine the number of buttons
            
            HIDP_CAPS caps;
            NTSTATUS status = HidP_GetCaps(preparsedData, &caps);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get capabilities for input.");

            u16 capabilityLength = caps.NumberInputButtonCaps;
            PHIDP_BUTTON_CAPS buttonCapabilities = (PHIDP_BUTTON_CAPS)FrameAlloc(sizeof(HIDP_BUTTON_CAPS) * capabilityLength);
            status = HidP_GetButtonCaps(HidP_Input, buttonCapabilities, &capabilityLength, preparsedData);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get button capabilities for input.");
            
            u32 numberOfButtons = buttonCapabilities->Range.UsageMax - buttonCapabilities->Range.UsageMin + 1;
                                    
            u32 usageLength = numberOfButtons;
            USAGE *usages = FrameAlloc(usageLength * sizeof(USAGE));
            
            status = HidP_GetUsages(HidP_Input, buttonCapabilities->UsagePage, 0, usages, &usageLength, preparsedData, 
                (char*)device->bRawData, device->dwSizeHid);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get button usages for device.");

            // NOTE(roger): flush button states on each event. This is how we know if the button was pressed or released.
            memset(buttonStates, 0, BUTTON_COUNT * sizeof(bool));
            for (u32 i = 0; i < usageLength; i++) {
                u32 index = usages[i] - buttonCapabilities->Range.UsageMin;
                buttonStates[index] = true;
                
                // NOTE(roger): Purely for logging purposes.
                const char* buttonLabel = 0;
                switch (index) {
                    case Xbox_ButtonA:     { buttonLabel = "A";            } break;
                    case Xbox_ButtonB:     { buttonLabel = "B";            } break;
                    case Xbox_ButtonX:     { buttonLabel = "X";            } break;
                    case Xbox_ButtonY:     { buttonLabel = "Y";            } break;
                    case Xbox_LeftBumper:  { buttonLabel = "Left Bumper";  } break;
                    case Xbox_RightBumper: { buttonLabel = "Right Bumper"; } break;
                    case Xbox_Start:       { buttonLabel = "Start";        } break;
                    case Xbox_Select:      { buttonLabel = "Select";       } break;
                    case Xbox_LeftStick:   { buttonLabel = "Left Stick";   } break;
                    case Xbox_RightStick:  { buttonLabel = "Right Stick";  } break;
                    case Xbox_Home:        { buttonLabel = "Home";         } break;
                
                    default : {
                        printf("Button Index: %u\n", index);
                    }
                }
                
                printf("Button: %s\n", buttonLabel);
            }
            
            // Extract value for input.
            
            capabilityLength = caps.NumberInputValueCaps;
            PHIDP_VALUE_CAPS valueCaps = (PHIDP_VALUE_CAPS)FrameAlloc(sizeof(HIDP_VALUE_CAPS) * capabilityLength);
            status = HidP_GetValueCaps(HidP_Input, valueCaps, &capabilityLength, preparsedData);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get value capabilities for input.");

            for (int i = 0; i < caps.NumberInputValueCaps; i++) {                
                HIDP_VALUE_CAPS *valueCap = &valueCaps[i];
                
                if (valueCap->IsRange) {
                    // TODO(roger): Implement? So far all value capabilities return IsRange = false.
                    //
                    // If IsRange is true, then you want to use valueCap->Range.UsageMin / UsageMax.
                } else {
                    u16 usage = valueCap->NotRange.Usage;
                    
                    u32 value = 0;
                    status = HidP_GetUsageValue(HidP_Input, valueCap->UsagePage, 0, usage, &value, 
                        preparsedData, device->bRawData, device->dwSizeHid);
                    Assert(status == HIDP_STATUS_SUCCESS, "Failed to get value for button.");
                    
                    // Now interpret usage + value
                    switch (usage) {
                        case 0x30: {
                            printf("Left Stick X: %u\n", value);
                        } break;
                            
                        case 0x31: {
                            printf("Left Stick Y: %u\n", value);
                        } break;
                        
                        case 0x33: {
                            printf("Right Stick X (Rx): %u\n", value);
                        } break;
                        
                        case 0x34: {
                            printf("Right Stick Y (Ry): %u\n", value);
                        } break;
                        
                        case 0x32: {
                            // This updates for both triggers. The value range changes based on if its left or right trigger.
                            // For example, Right Trigger return values between 32768 - 128. 
                            // And Left Trigger returns values between 32768 - 65408.

                            printf("Z Trigger: %u\n", value);
                        } break;
                        
                        case 0x39: {
                            printf("Hat Switch (D-Pad): %u\n", value);
                        } break;
                        
                        default: {
                            printf("Unknown Usage 0x%X => %lu\n", usage, value);
                        } break;                        
                    }
                }
            }
        } break;
        
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;
    
        default: {
            return DefWindowProc(window, message, wParam, lParam);
        }
    }
    
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow) {
    if (AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        printf("Console attached.\n");
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = instance;
    wc.lpszClassName = TEXT("RawInputExample");
    RegisterClass(&wc);

    HWND window = CreateWindow(
        wc.lpszClassName,
        TEXT("Raw Input Gamepad Example"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        0, 0, instance, 0);

    ShowWindow(window, cmdShow);
    UpdateWindow(window);

    // Register the gamepad as a Raw Input device
    RAWINPUTDEVICE rid;
    
    // See this page for possible usage values:
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-architecture#hid-clients-supported-in-windows
    
    rid.usUsagePage = 0x0001;
    rid.usUsage     = 0x0004 | 0x0005;
    rid.dwFlags     = 0;
    rid.hwndTarget  = window;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        printf("Failed to register raw input device");
        return 1;
    }

    // Main Loop
    MSG message;
    while (GetMessage(&message, 0, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}
