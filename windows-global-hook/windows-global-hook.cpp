#include "stdafx.h"

#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <sstream>

#pragma comment(lib, "Shcore.lib") 

using namespace std;

class GraphicsManager {
	// TODO: fetch scale dynamically
	static const int screenScaleX = 2;
	static const int screenScaleY = 2;
public:
	GraphicsManager() {
		init();
	}

	SIZE getScreenSize() {
		if (_screenSize.cx > 0 && _screenSize.cy > 0) {
			return _screenSize;
		}
		else {
			SIZE sz;
			sz.cx = GetDeviceCaps(_hdcScreen, HORZRES) * screenScaleX;
			sz.cy = GetDeviceCaps(_hdcScreen, VERTRES) * screenScaleY;
			return sz;
		}
	}

	void captureRegion(RECT r) {
		int x = r.right - r.left;
		int y = r.bottom - r.top;

		HDC hdcMem = CreateCompatibleDC(_hdcScreen);
		_hBitmapRegion = CreateCompatibleBitmap(_hdcScreen, x, y);
		HGDIOBJ hOld = SelectObject(hdcMem, _hBitmapRegion);
		BitBlt(hdcMem, r.left, r.top, x, y, _hdcScreen, 0, 0, SRCCOPY);
		SelectObject(hdcMem, hOld);

		BITMAPINFOHEADER bmi = { 0 };
		bmi.biSize = sizeof(BITMAPINFOHEADER);
		bmi.biPlanes = 1;
		bmi.biBitCount = 32;
		bmi.biWidth = x;
		bmi.biHeight = -y;
		bmi.biCompression = BI_RGB;
		bmi.biSizeImage = 0;// 3 * screenSize.cx * ScreenY;

		if (_screenData) {
			free(_screenData);
		}

		_screenData = (BYTE*)malloc(4 * x * y);

		GetDIBits(hdcMem, _hBitmapRegion, 0, y, _screenData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
	}

	void captureScreen()
	{
		captureRegion({ 0,0,_screenSize.cx, _screenSize.cy });
	}

	PBITMAPINFO CreateBitmapInfoStruct(HBITMAP hBmp)
	{
		BITMAP bmp;
		PBITMAPINFO pbmi;
		WORD    cClrBits;

		// Retrieve the bitmap color format, width, and height.  
		assert(GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp));

		// Convert the color format to a count of bits.  
		cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
		if (cClrBits == 1)
			cClrBits = 1;
		else if (cClrBits <= 4)
			cClrBits = 4;
		else if (cClrBits <= 8)
			cClrBits = 8;
		else if (cClrBits <= 16)
			cClrBits = 16;
		else if (cClrBits <= 24)
			cClrBits = 24;
		else cClrBits = 32;

		// Allocate memory for the BITMAPINFO structure. (This structure  
		// contains a BITMAPINFOHEADER structure and an array of RGBQUAD  
		// data structures.)  

		if (cClrBits < 24)
			pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
				sizeof(BITMAPINFOHEADER) +
				sizeof(RGBQUAD) * (1 << cClrBits));

		// There is no RGBQUAD array for these formats: 24-bit-per-pixel or 32-bit-per-pixel 

		else
			pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
				sizeof(BITMAPINFOHEADER));

		// Initialize the fields in the BITMAPINFO structure.  

		pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmi->bmiHeader.biWidth = bmp.bmWidth;
		pbmi->bmiHeader.biHeight = bmp.bmHeight;
		pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
		pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
		if (cClrBits < 24)
			pbmi->bmiHeader.biClrUsed = (1 << cClrBits);

		// If the bitmap is not compressed, set the BI_RGB flag.  
		pbmi->bmiHeader.biCompression = BI_RGB;

		// Compute the number of bytes in the array of color  
		// indices and store the result in biSizeImage.  
		// The width must be DWORD aligned unless the bitmap is RLE 
		// compressed. 
		pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8
			* pbmi->bmiHeader.biHeight;
		// Set biClrImportant to 0, indicating that all of the  
		// device colors are important.  
		pbmi->bmiHeader.biClrImportant = 0;
		return pbmi;
	}

	void saveBitmapToFile(HBITMAP hBMP, LPCTSTR pszFile)
	{
		HANDLE hf;                 // file handle  
		BITMAPFILEHEADER hdr;       // bitmap file-header  
		PBITMAPINFOHEADER pbih;     // bitmap info-header  
		LPBYTE lpBits;              // memory pointer  
		DWORD dwTotal;              // total count of bytes  
		DWORD cb;                   // incremental count of bytes  
		BYTE *hp;                   // byte pointer  
		DWORD dwTmp;
		PBITMAPINFO pbi;
		HDC hDC;

		hDC = CreateCompatibleDC(GetWindowDC(GetDesktopWindow()));
		SelectObject(hDC, hBMP);

		pbi = CreateBitmapInfoStruct(hBMP);

		pbih = (PBITMAPINFOHEADER)pbi;
		lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

		printf("bitmap (width=%d,height=%d)\n", pbih->biWidth, pbih->biHeight);

		assert(lpBits);

		// Retrieve the color table (RGBQUAD array) and the bits  
		// (array of palette indices) from the DIB.  
		assert(GetDIBits(hDC, hBMP, 0, (WORD)pbih->biHeight, lpBits, pbi,
			DIB_RGB_COLORS));

		// Create the .BMP file.  
		hf = CreateFile(pszFile,
			GENERIC_READ | GENERIC_WRITE,
			(DWORD)0,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			(HANDLE)NULL);
		assert(hf != INVALID_HANDLE_VALUE);

		hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"  
									// Compute the size of the entire file.  
		hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) +
			pbih->biSize + pbih->biClrUsed
			* sizeof(RGBQUAD) + pbih->biSizeImage);
		hdr.bfReserved1 = 0;
		hdr.bfReserved2 = 0;

		// Compute the offset to the array of color indices.  
		hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) +
			pbih->biSize + pbih->biClrUsed
			* sizeof(RGBQUAD);

		// Copy the BITMAPFILEHEADER into the .BMP file.  
		assert(WriteFile(hf, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER),
			(LPDWORD)&dwTmp, NULL));

		// Copy the BITMAPINFOHEADER and RGBQUAD array into the file.  
		assert(WriteFile(hf, (LPVOID)pbih, sizeof(BITMAPINFOHEADER)
			+ pbih->biClrUsed * sizeof(RGBQUAD),
			(LPDWORD)&dwTmp, (NULL)));

		// Copy the array of color indices into the .BMP file.  
		dwTotal = cb = pbih->biSizeImage;
		hp = lpBits;
		assert(WriteFile(hf, (LPSTR)hp, (int)cb, (LPDWORD)&dwTmp, NULL));

		// Close the .BMP file.  
		assert(CloseHandle(hf));

		// Free memory.  
		GlobalFree((HGLOBAL)lpBits);
	}

	RGBTRIPLE getPixelColor(POINT p) {
		RGBTRIPLE c;
		c.rgbtBlue = _screenData[4 * ((p.y*_screenSize.cx) + p.x)];
		c.rgbtGreen = _screenData[4 * ((p.y*_screenSize.cx) + p.x) + 1];
		c.rgbtRed = _screenData[4 * ((p.y*_screenSize.cx) + p.x) + 2];
		return c;
	}

	float rgbColorDistance(RGBTRIPLE c1, RGBTRIPLE c2) {
		byte c1Red = c1.rgbtRed;
		byte c1Green = c1.rgbtGreen;
		byte c1Blue = c1.rgbtBlue;
	
		byte c2Red = c2.rgbtRed;
		byte c2Green = c2.rgbtGreen;
		byte c2Blue = c2.rgbtBlue;
	
		int diffRed = abs(c1Red - c2Red);
		int diffGreen = abs(c1Green - c2Green);
		int diffBlue = abs(c1Blue - c2Blue);
		float pctDiffRed = (float)diffRed / 255;
		float pctDiffGreen = (float)diffGreen / 255;
		float pctDiffBlue = (float)diffBlue / 255;
		return ((pctDiffRed + pctDiffGreen + pctDiffBlue) / 3);
	}

	RGBTRIPLE HexColorStringToRGB(LPCTSTR input)
	{
		LPTSTR pStop;
		int num = _tcstol(input + 1, &pStop, 16);
		int r = (num & 0xFF0000) >> 16;
		int g = (num & 0xFF00) >> 8;
		int b = num & 0xFF;
		return { (byte)b,(byte)g,(byte)r };
	}

	vector<RGBTRIPLE> loadTargetColors() {

		struct Utl {
			static vector<string> split(const string &s, char delim) {
				stringstream ss(s);
				string item;
				vector<string> tokens;
				while (getline(ss, item, delim)) {
					tokens.push_back(item);
				}
				return tokens;
			}

			static wstring string2wstring(const std::string& s)
			{
				int len;
				int slength = (int)s.length() + 1;
				len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
				wchar_t* buf = new wchar_t[len];
				MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
				std::wstring r(buf);
				delete[] buf;
				return r;
			}

		};

		vector<RGBTRIPLE> colors;
		vector<string> hexColorList = Utl::split(_targetHexColorListString, ',');
		for (auto &hexColor : hexColorList) {
			stringstream ss("#");
			ss << "#" << hexColor;
			RGBTRIPLE color = HexColorStringToRGB(Utl::string2wstring(ss.str()).c_str());
			colors.push_back(color);
		}
		return colors;
	}

	void pixelSearch() {
		int radius = 50;

		int midX = _screenSize.cx / 2;
		int midY = _screenSize.cy / 2;

		int minX = midX - radius;
		int minY = midY - radius;
		int maxX = midX + radius;
		int maxY = midY + radius;

		printf("minX=%d, minY=%d, maxX=%d, maxY=%d\n", minX, minY, maxX, maxY);
		for (int x = minX; x < maxX; x++) {
			for (int y = minY; y < maxY; y++) {
				RGBTRIPLE c = getPixelColor({ x,y });
				byte r = c.rgbtRed;
				byte g = c.rgbtGreen;
				byte b = c.rgbtBlue;

				//for (int i = 0; i < sizeof(pixelColors) / sizeof(RGB); i++) {
				for (auto matchPixelColor : _targetRGBColorList) {
					//RGB matchPixelColor = pixelColors[i];
					RGBTRIPLE sourcePixelColor = { r,g,b };
					float colorDistance = rgbColorDistance(matchPixelColor, sourcePixelColor);

					//if (r == pixelColor.r && g == pixelColor.g && b == pixelColor.b) {
					if (colorDistance < 0.1) {
						printf("found match.  colorDistance=%.2f\n", colorDistance);
						int xFactor = 5;
						int yFactor = 5;
						int dirX = -1;
						int dirY = -1;
						if (x < midX) {
							dirX = 1;
						}
						if (y < midY) {
							dirY = 1;
						}

						dirX = dirX * xFactor;
						dirY = dirY * yFactor;

						mouse_event(MOUSEEVENTF_MOVE, dirX, dirY, NULL, NULL);
						return;

					}
				}
			}
		}
	}

	void testSaveBitmapToFile() {
		captureScreen();
		saveBitmapToFile(_hBitmapRegion, _T("c:\\temp\\screen.bmp"));

		captureRegion({ 0,0,500,500 });
		saveBitmapToFile(_hBitmapRegion, _T("c:\\temp\\region.bmp"));
	}

private:
	HWND _hwndDesktopWindow = 0;
	HDC _hdcScreen = 0;
	SIZE _screenSize = { 0,0 };
	HBITMAP _hBitmapRegion = 0;
	BYTE* _screenData = 0;

	string _targetHexColorListString = "2C221E,8F6B51,2C1916,DAB790,7B62AD,2E1B40,4C3131,946DBB,3E2B2B,997560,C7A796,EDE1C7,512E32,874742,3C3B3D,6C574C,4D4943,8B815E,5D312F,1B1A1C,CC825C,C9764B,8C5C3D,98634A,364347,403C37,929282";
	vector<RGBTRIPLE> _targetRGBColorList;

	void init() {
		_hwndDesktopWindow = GetDesktopWindow();
		_hdcScreen = GetDC(_hwndDesktopWindow);
		_screenSize = getScreenSize();
		_targetRGBColorList = loadTargetColors();
	}
};

class KeyboardAndMouseHookManager {
public:
	KeyboardAndMouseHookManager(GraphicsManager* graphicsManager) {
		_graphicsManager = graphicsManager;
		init();
	}

private:
	static GraphicsManager* _graphicsManager;
	static HHOOK _hMouseHook;
	static MOUSEHOOKSTRUCT* _pMouseStruct;
	static HHOOK _hKeyboardHook;
	static KBDLLHOOKSTRUCT _kbdStruct;
	static BOOL _rightMouseButtonDown;

	void init() {
		_rightMouseButtonDown = false;

		HANDLE hThreadMouseHook;
		DWORD dwThreadMouseHook;

		hThreadMouseHook = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)SetMouseHook, NULL, NULL, &dwThreadMouseHook);

		DWORD dwThreadMouseActions;
		HANDLE hThreadMouseActions = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)MouseActions, NULL, NULL, &dwThreadMouseActions);

		DWORD dwThreadKeyboardHook;
		HANDLE hKeyboardHook = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)SetKeyboardHook, NULL, NULL, &dwThreadKeyboardHook);

		if (hThreadMouseHook) {
			WaitForSingleObject(hThreadMouseHook, INFINITE);
		}

		if (hThreadMouseActions) {
			WaitForSingleObject(hThreadMouseActions, INFINITE);
		}

		if (hKeyboardHook) {
			WaitForSingleObject(hKeyboardHook, INFINITE);
		}
	}

	static LRESULT CALLBACK MouseHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
	{
		_pMouseStruct = (MOUSEHOOKSTRUCT *)lParam;
		if (_pMouseStruct != NULL) {
			if (wParam == WM_RBUTTONDOWN)
			{
				_rightMouseButtonDown = true;
			}
			if (wParam == WM_RBUTTONUP)
			{
				_rightMouseButtonDown = false;
			}

		}
		return CallNextHookEx(_hMouseHook, nCode, wParam, lParam);
	}

	static DWORD WINAPI SetMouseHook(LPVOID lpParm)
	{
		HINSTANCE hInstance = GetModuleHandle(NULL);
		_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookCallback, hInstance, NULL);

		MSG message;
		while (GetMessage(&message, NULL, 0, 0)) {
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		UnhookWindowsHookEx(_hMouseHook);
		return 0;
	}

	static DWORD WINAPI MouseActions(LPVOID lpParm) {

		int counter = 0;

		while (true) {
			if (_rightMouseButtonDown) {
				//saveScreenToBitmap(pMouseStruct);
				//printf("Mouse position X = %d  Mouse Position Y = %d\n", pMouseStruct->pt.x, pMouseStruct->pt.y);

				//printf("clicked");
				//mouse_event(MOUSEEVENTF_MOVE, 50, 50, NULL, NULL);
				printf("counter=%d\n", counter++);
				//pixelSearch();
			}
			else {
				counter = 0;
			}
		}
		return 0;
	}

	static LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode >= 0)
		{
			if (wParam == WM_KEYDOWN)
			{
				_kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
				
				if (_kbdStruct.vkCode == VK_UP)
				{
					printf("up arrow pressed!");
					_graphicsManager->testSaveBitmapToFile();
				}
			}
		}

		// call the next hook in the hook chain. This is nessecary or your hook chain will break and the hook stops
		return CallNextHookEx(_hKeyboardHook, nCode, wParam, lParam);
	}

	static DWORD WINAPI SetKeyboardHook()
	{
		HINSTANCE hInstance = GetModuleHandle(NULL);
		if (!(_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, hInstance, 0)))
		{
			printf("Failed to install keyuboard hook");
		}

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UnhookWindowsHookEx(_hKeyboardHook);
		return 0;
	}
};

GraphicsManager* KeyboardAndMouseHookManager::_graphicsManager = NULL;
HHOOK KeyboardAndMouseHookManager::_hMouseHook = 0;
MOUSEHOOKSTRUCT* KeyboardAndMouseHookManager::_pMouseStruct = 0;
HHOOK KeyboardAndMouseHookManager::_hKeyboardHook = 0;
KBDLLHOOKSTRUCT KeyboardAndMouseHookManager::_kbdStruct = {};
BOOL KeyboardAndMouseHookManager::_rightMouseButtonDown = false;

int main(int argc, char** argv)
{
	GraphicsManager* graphicsManager = new GraphicsManager();
	KeyboardAndMouseHookManager* hookManager = new KeyboardAndMouseHookManager(graphicsManager);
}


//bool ButtonPress(int Key)
//{
//	bool button_pressed = false;
//
//	while (GetAsyncKeyState(Key))
//		button_pressed = true;
//
//	return button_pressed;
//}

//static void screenshot() {
//	// get the device context of the screen
//	HDC hScreenDC = CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
//	// and a device context to put it in
//	HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
//
//	int width = GetDeviceCaps(hScreenDC, HORZRES);
//	int height = GetDeviceCaps(hScreenDC, VERTRES);
//
//	// maybe worth checking these are positive values
//	HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
//
//	// get a new bitmap
//	HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
//
//	BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
//	hBitmap = (HBITMAP)SelectObject(hMemoryDC, hOldBitmap);
//
//	// clean up
//	DeleteDC(hMemoryDC);
//	DeleteDC(hScreenDC);
//}

//int scalingFactorX = GetDeviceCaps(hScreen, LOGPIXELSX);
//printf("LOGPIXELSX=%d,HORZSIZE=%d\n", GetDeviceCaps(hScreen, LOGPIXELSX), GetDeviceCaps(hScreen, HORZSIZE));

//HMONITOR hMonitor;
//POINT    pt;
//UINT     dpix = 0, dpiy = 0;
//HRESULT  hr = E_FAIL;
//
//// Get the DPI for the main monitor, and set the scaling factor
//pt.x = 1;
//pt.y = 1;
//hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
//hr = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
//printf("");