#include <windows.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit;
    char buffer[256];

    switch (msg) {
    case WM_CREATE:
        // Create a single-line text box
        hEdit = CreateWindowEx(
            0, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 10, 200, 25,
            hwnd, (HMENU)1, GetModuleHandle(NULL), NULL
        );
        // Create an OK button
        CreateWindow("BUTTON", "OK",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            220, 10, 80, 25,
            hwnd, (HMENU)2, GetModuleHandle(NULL), NULL
        );
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 2) { // OK button
            GetWindowText(hEdit, buffer, sizeof(buffer));
            MessageBox(hwnd, buffer, "You typed:", MB_OK);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TextboxExample";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, "Text Box Example",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 100,
        NULL, NULL, hInstance, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
