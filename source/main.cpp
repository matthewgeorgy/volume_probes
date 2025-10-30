#define MG_USE_WINDOWS_H
#include <mg.h>
#include <dxgi.h>
#include <d3d11.h>
#include <stdio.h>

#define SCR_WIDTH 		1024
#define SCR_HEIGHT		768

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int
main()
{
	//////////////////////////////////////////////////////////////////////////
	// Win32 setup

	ATOM Atom;
	WNDCLASSEX WndClass = {};
	HWND Window;
	MSG Message;
	RECT WindowRect;
	INT WindowWidth, WindowHeight;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	WndClass.lpfnWndProc = &WndProc;
	WndClass.lpszClassName = "VolumeProbesWindowClass";
	WndClass.hInstance = GetModuleHandleA(NULL);

	Atom = RegisterClassEx(&WndClass);
	if (!Atom)
	{
		printf("Failed to create register window class...!r\n");
		return (-1);
	}

	WindowRect.left = 0;
	WindowRect.right = SCR_WIDTH;
	WindowRect.top = 0;
	WindowRect.bottom = SCR_HEIGHT;

	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

	WindowWidth = WindowRect.right - WindowRect.left;
	WindowHeight = WindowRect.bottom - WindowRect.top;

	Window = CreateWindowEx(0, WndClass.lpszClassName, "Volume Probes",
		WS_OVERLAPPEDWINDOW, 0, 0, WindowWidth, WindowHeight,
		NULL, NULL, WndClass.hInstance, NULL);
	if (!Window)
	{
		printf("Failed to create window...!\r\n");
		return (-1);
	}

	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);

	//////////////////////////////////////////////////////////////////////////
	// Main loop

	for (;;)
	{
		if (PeekMessage(&Message, Window, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT)
			{
				break;
			}
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
		else
		{
		}
	}

	return (0);
}

LRESULT CALLBACK
WndProc(HWND hWnd,
		UINT Msg,
		WPARAM wParam,
		LPARAM lParam)
{
	LRESULT Result = 0;

	switch (Msg)
	{
		case WM_KEYDOWN:
		{
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
		} break;

		case WM_CLOSE:
		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;

		default:
		{
			Result = DefWindowProc(hWnd, Msg, wParam, lParam);
		} break;
	}

	return (Result);
}

