#include <windows.h>
#include <stdio.h>
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

#define Assert(condition, message, ...) \
    do { \
        if (!(condition)) { \
            sprintf(ASSERT_MESSAGE, message, __VA_ARGS__); \
            sprintf(ASSERT_BOX_MESSAGE, "ERROR: %s\n\tAssertion failed: %s\n\tFile: %s, Line: %d\n", \
                ASSERT_MESSAGE, #condition, __FILE__, __LINE__); \
            MessageBoxA(0, ASSERT_BOX_MESSAGE, "Assert", MB_OK | MB_ICONERROR); \
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
            
            HIDP_CAPS capabilities;
            NTSTATUS status = HidP_GetCaps(preparsedData, &capabilities);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get capabilities for input.");

            u16 capabilityLength = capabilities.NumberInputButtonCaps;
            PHIDP_BUTTON_CAPS buttonCapabilities = (PHIDP_BUTTON_CAPS)FrameAlloc(sizeof(HIDP_BUTTON_CAPS) * capabilityLength);
            status = HidP_GetButtonCaps(HidP_Input, buttonCapabilities, &capabilityLength, preparsedData);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get button capabilities for input.");
            
            u32 numberOfButtons = buttonCapabilities->Range.UsageMax - buttonCapabilities->Range.UsageMin + 1;
            
            // Now get the value capability array. This array specifies capabailities of HID controls that can more than two states.
            // These controls often have a range of values. 
            //
            // For example, an analog stick (four axes) can have a range of 0x00 to 0xFF where 0x80 equals centered position of the stick.
            
            capabilityLength = capabilities.NumberInputValueCaps;
            PHIDP_VALUE_CAPS valueCapabilities = (PHIDP_VALUE_CAPS)FrameAlloc(sizeof(HIDP_VALUE_CAPS) * capabilityLength);
            status = HidP_GetValueCaps(HidP_Input, valueCapabilities, &capabilityLength, preparsedData);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get value capabilities for input.");
                                    
            u32 usageLength = numberOfButtons;
            USAGE *usages = FrameAlloc(usageLength * sizeof(USAGE));
            
            status = HidP_GetUsages(HidP_Input, buttonCapabilities->UsagePage, 0, usages, &usageLength, preparsedData, 
                (char*)device->bRawData, device->dwSizeHid);
            Assert(status == HIDP_STATUS_SUCCESS, "Failed to get button usages for device.");
            
            // 'usages' can be interpreted to figure out which input is changing value. 
            // For a simple button, this can mean pressed or released.
            // For a trigger, this can be a range based on pressure.
            // For a stick, this can be the range for an axis.
            // The value of the input can be extracted from the 'valueCapabilities'.
            
            // TODO(roger): How does D-Pad work? Pressing a D-Pad input changes the 'NumberInputValueCaps', but does not effect usageLength.
            
            const char* buttonLabel = 0;
            for (u32 i = 0; i < usageLength; i++) {
                u32 index = usages[i] - buttonCapabilities->Range.UsageMin;                
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
            
            // Extract value for input. Released, pressed, etc.

            // TODO(roger): Time to figure out how this works...
            // capabilities.NumberInputValueCap changes to 6 when pressing a button.
                        
            for (int i = 0; i < capabilities.NumberInputValueCaps; i++) {                
                u32 value = 0;
                status = HidP_GetUsageValue(HidP_Input, valueCapabilities[i].UsagePage, 0, valueCapabilities[i].Range.UsageMin, &value, 
                    preparsedData, device->bRawData, device->dwSizeHid);
                Assert(status == HIDP_STATUS_SUCCESS, "Failed to get value for button.");

                printf("UsageMin: %hu, Value: %u\n", valueCapabilities[i].Range.UsageMin, value);
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
