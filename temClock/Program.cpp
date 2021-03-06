#include <stdio.h>
#include <limits.h>
#include <Windows.h>
#include <tchar.h>
#include <WindowsX.h>
#include "MaiTimer.h"
#include "DwmFrame.h"
#include "MyWinAPI/Uxtheme.h"
#include "MyWinAPI/Create32bitBmp.h"
#include "resource.h"

typedef int			i32_t;
#define I32_MAX		INT_MAX
#define I32_MIN		INT_MIN

#define CLASS_NAME	_T("MaiSoft.temClock")
#define WND_TITLE	_T("temClock")
#define WND_STYLE	(WS_OVERLAPPEDWINDOW)
#define WIDTH_OF(rc)  ((rc).right - (rc).left)
#define HEIGHT_OF(rc) ((rc).bottom - (rc).top)
#define INI_FILENAME	_T("temClock.ini")

#define FORECOLOR		RGB(  0,  0,  0)
#define STR_SIZE		(1024)

int dpi_y;
HINSTANCE hInst;
HWND hWnd;
HFONT hFont, hFontSmall;
LOGFONT lfFont, lfFontSmall;
TCHAR *ontimeout_exec, *ontimeout_sound, *ini_filepath; // never NULL
bool dwm_extend_frame;
volatile bool timer_mode, stopw_mode;
volatile i32_t timer_initial; // initial ms, before timer start
volatile UINT timer_id; // from SetTimer()
volatile i32_t stopw_lap1, stopw_lap2, stopw_lap3;
MaiTimer timer, stopw;
DwmFrame dwmf;

volatile COLORREF bgcolor;
struct OMO_STATE {
	volatile BYTE r, g, b;
	volatile bool r_inc, g_inc, b_inc;
} omo = { 80, 200, 200, false, false, false };

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
VOID CALLBACK UpdateUiProc(HWND, UINT, UINT, DWORD);
VOID CALLBACK AdvanceProc(HWND, UINT, UINT, DWORD);
VOID CALLBACK OnTimerTimeout(HWND, UINT, UINT, DWORD);
void OnPaint(HWND);
void OnKey(HWND, UINT, BOOL, int, UINT);
void OnRButtonUp(HWND, int, int, UINT);
UINT OnNcHitTest(HWND hwnd, int x, int y);
void stopw_StartPause();
void stopw_Reset();
void stopw_Lap();
void timer_StartPause();
void timer_Reset();
void timer_SetTimeout();
void CenterParent(HWND, HWND);
void MsToMulti(i32_t, i32_t*, i32_t*, i32_t*, i32_t*);
void PrintElapsedTime(TCHAR*, i32_t, bool show_ms = true);
bool TryParseTextOf(HWND, int ctlId, i32_t*);
bool TryAdd(i32_t, i32_t*);
bool TryMultiply(i32_t, i32_t*);
void ShowSettingsDlg();
void LoadSaveIni(HWND, bool);
void MyDrawText(HDC, LPTSTR, LPRECT, UINT);

// Timespan dialog ...
BOOL CALLBACK TimespanDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL dlgTimespan_OnCommand(HWND, int, HWND, UINT);
BOOL dlgTimespan_OnInitDialog(HWND, HWND, LPARAM);
void dlgTimespan_OnBtnOK(HWND hDlg);
// Settings dialog ...
LOGFONT dlgSettings_lfFont, dlgSettings_lfFontSmall;
HFONT dlgSettings_hFontWebdings;
BOOL CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL dlgSettings_OnCommand(HWND, int, HWND, UINT);
BOOL dlgSettings_OnInitDialog(HWND, HWND, LPARAM);
void dlgSettings_UpdateCtrl(HWND);
void dlgSettings_OnCheckBoxToggled(HWND);
void dlgSettings_OnBtnOK(HWND);
void dlgSettings_OnBtnSaveINI(HWND);
void dlgSettings_OnBtnLoadINI(HWND);
void dlgSettings_OnBtnSetFont(HWND);
void dlgSettings_OnBtnSetMiniFont(HWND);
void dlgSettings_OnBtnBrowseExe(HWND);
void dlgSettings_OnBtnBrowseSound(HWND);
void dlgSettings_OnBtnTestSound(HWND);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow) {
	bgcolor = RGB(omo.r, omo.g, omo.b);
	{
		HDC hdc = GetDC(NULL);
		dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(NULL, hdc);
	}{
		_tcscpy(lfFont.lfFaceName, _T("Arial"));
		lfFont.lfHeight = -MulDiv(24, dpi_y, 72);
		hFont = CreateFontIndirect(&lfFont);
		_tcscpy(lfFontSmall.lfFaceName, _T("Arial"));
		lfFontSmall.lfHeight = -MulDiv(14, dpi_y, 72);
		hFontSmall = CreateFontIndirect(&lfFontSmall);
		dlgSettings_hFontWebdings = CreateFont(
			-MulDiv(11, dpi_y, 72),0,0,0,0,0,0,0,
			SYMBOL_CHARSET,0,0,0,0,_T("Webdings"));
	}{
		ontimeout_exec = new TCHAR[MAX_PATH];
		*ontimeout_exec = '\0';
		ontimeout_sound = new TCHAR[MAX_PATH];
		*ontimeout_sound = '\0';
		ini_filepath = new TCHAR[MAX_PATH];
		TCHAR exepath[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH];
		GetModuleFileName(GetModuleHandle(NULL), exepath, MAX_PATH);
		_tsplitpath(exepath, drive, dir, NULL, NULL);
		_stprintf(ini_filepath, _T("%s%s%s"), drive, dir, INI_FILENAME);
	}
	hInst = hInstance;
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	wcex.hbrBackground = (HBRUSH)(COLOR_BTNSHADOW);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpfnWndProc = WndProc;
	wcex.lpszClassName = CLASS_NAME;
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClassEx(&wcex)) { return 1; }
	hWnd = CreateWindowEx(0,
		CLASS_NAME,
		WND_TITLE,
		WND_STYLE,
		CW_USEDEFAULT, 0,
		600, 400,
		NULL, NULL,
		NULL, NULL);
	if (!hWnd) { return 2; }
	dwmf.SetHwnd(hWnd);
	ShowWindow(hWnd, nCmdShow);
	{
		SetTimer(NULL, 0, 0, UpdateUiProc);
		SetTimer(NULL, 0, 20, AdvanceProc);
	}
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	// Cleanup code omitted, as the program is exiting already
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
	switch (msg) {
	case WM_ERASEBKGND: break;
	case WM_PAINT: HANDLE_WM_PAINT(hWnd, w, l, OnPaint); break;
	case WM_KEYDOWN: HANDLE_WM_KEYDOWN(hWnd, w, l, OnKey); break;
	//case WM_LBUTTONUP: HANDLE_WM_LBUTTONUP(hWnd, w, l, OnLButtonUp); break;
	case WM_NCRBUTTONUP:
	case WM_RBUTTONUP: HANDLE_WM_RBUTTONUP(hWnd, w, l, OnRButtonUp); break;
	case WM_DWMCOMPOSITIONCHANGED: dwmf.OnDwmCompositionChanged(); break;
	case WM_NCHITTEST: return HANDLE_WM_NCHITTEST(hWnd, w, l, OnNcHitTest);
	case WM_DESTROY: PostQuitMessage(0); break;
	default: return DefWindowProc(hWnd, msg, w, l);
	}
	return 0;
}

VOID CALLBACK AdvanceProc(HWND, UINT, UINT, DWORD) {
	if (omo.r_inc) { omo.r += 2; }	else { omo.r -= 2; }
	if (omo.g_inc) { ++omo.g; }	else { --omo.g; }
	//if (omo.b_inc) { ++omo.b; }	else { --omo.b; }
	if (omo.r >= 250) { omo.r_inc = false; }
	if (omo.r <= 100) { omo.r_inc = true;  }
	if (omo.g >= 250) { omo.g_inc = false; }
	if (omo.g <= 100) { omo.g_inc = true;  }
	//if (omo.b >= 250) { omo.b_inc = false; }
	//if (omo.b <= 100) { omo.b_inc = true;  }
	bgcolor = RGB(omo.r, omo.g, omo.b);
}

VOID CALLBACK UpdateUiProc(HWND, UINT, UINT, DWORD) {
	InvalidateRect(hWnd, NULL, TRUE);
}

VOID CALLBACK OnTimerTimeout(HWND, UINT, UINT id, DWORD) {
	timer_Reset();
	BOOL want_mute = FALSE;
	if (*ontimeout_sound) {
		want_mute = PlaySound(ontimeout_sound, NULL,
			SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
	}
	if (*ontimeout_exec) {
		TCHAR *appname = NULL, *cmdline = NULL;
		// if exists, treat as appname, else treat as cmdline
		WIN32_FIND_DATA fdata;
		ZeroMemory(&fdata, sizeof(fdata));
		HANDLE h = FindFirstFile(ontimeout_exec, &fdata);
		if (h != INVALID_HANDLE_VALUE) { appname = ontimeout_exec; }
		else { cmdline = ontimeout_exec; }
		// run ...
		STARTUPINFO sinfo;
		ZeroMemory(&sinfo, sizeof(sinfo));
		sinfo.cb = sizeof(sinfo);
		sinfo.wShowWindow = SW_SHOWNORMAL;
		PROCESS_INFORMATION pinfo;
		ZeroMemory(&pinfo, sizeof(pinfo));
		CreateProcess(appname, cmdline, NULL, NULL,
			FALSE, 0, NULL, NULL, &sinfo, &pinfo);
	}
	if (!want_mute) {
		MessageBox(hWnd, _T("Timeout!"), _T("Timer"),
			MB_OK | MB_ICONINFORMATION);
	}
}

void OnPaint(HWND hWnd) {
	RECT rc;
	GetClientRect(hWnd, &rc);
	PAINTSTRUCT ps;
	HDC real_hdc = BeginPaint(hWnd, &ps);
	HDC hdc = CreateCompatibleDC(real_hdc);
	HBITMAP hbm = Create32bitBmp(hdc, WIDTH_OF(rc), HEIGHT_OF(rc), NULL);
	HGDIOBJ old_hbm = SelectObject(hdc, hbm);
	HGDIOBJ old_hfo = SelectObject(hdc, hFont);
	// Clear
	HBRUSH hbrBg = CreateSolidBrush(
		dwmf.IsExtended() ? RGB(0,0,0) : bgcolor);
	FillRect(hdc, &rc, hbrBg);
	DeleteObject(hbrBg);
	// Draw text
	TCHAR str[STR_SIZE] = {0};
	SetTextColor(hdc, FORECOLOR);
	SetBkMode(hdc, TRANSPARENT);
	if (timer_mode) {
		i32_t elapsed = timer.GetElapsedMs_i32();
		_tcscpy(str, _T("~ Timer ~\n"));
		PrintElapsedTime(&str[_tcslen(str)],
			elapsed > timer_initial ? 0 :
			timer_initial - elapsed);
	}
	else if (stopw_mode) {
		_tcscpy(str, _T("~ Stopwatch ~\n"));
		PrintElapsedTime(&str[_tcslen(str)], stopw.GetElapsedMs_i32());
		if (stopw_lap3) {
			_tcscat(str, _T("\nLap 3: "));
			PrintElapsedTime(&str[_tcslen(str)], stopw_lap3);
		}
		if (stopw_lap2) {
			_tcscat(str, _T("\nLap 2: "));
			PrintElapsedTime(&str[_tcslen(str)], stopw_lap2);
		}
		if (stopw_lap1) {
			_tcscat(str, _T("\nLap 1: "));
			PrintElapsedTime(&str[_tcslen(str)], stopw_lap1);
		}
	}
	else /* clock_mode */ {
		SYSTEMTIME time;
		GetLocalTime(&time);
		_stprintf(str,
			_T("%.2u:%.2u:%.2u\n")
			_T("%u/%u/%u "),
			time.wHour,
			time.wMinute,
			time.wSecond,
			time.wDay,
			time.wMonth,
			time.wYear);
		switch (time.wDayOfWeek) {
		case 0: _tcscat(str, _T("Sun")); break;
		case 1: _tcscat(str, _T("Mon")); break;
		case 2: _tcscat(str, _T("Tue")); break;
		case 3: _tcscat(str, _T("Wed")); break;
		case 4: _tcscat(str, _T("Thu")); break;
		case 5: _tcscat(str, _T("Fri")); break;
		case 6: _tcscat(str, _T("Sat")); break;
		}
	}	
	// Find out the Width and Height needed to store the text
	// When passing all-zero RECT for DT_CALCRECT, x and y will remain zero,
	// while .right and .bottom will be Width and Height.
	// Thus, we need to += them after calculating x and y.
	RECT rcText = {0};
	MyDrawText(hdc, str, &rcText, DT_CALCRECT);
	rcText.right += rcText.left = (WIDTH_OF(rc) - WIDTH_OF(rcText)) / 2;
	rcText.bottom += rcText.top = (HEIGHT_OF(rc) - HEIGHT_OF(rcText)) / 2;
	MyDrawText(hdc, str, &rcText, DT_CENTER);
	// If timer/stopwatch is running on background,
	// show their status on 'Clock' screen.
	if (!timer_mode && !stopw_mode) {
		*str = '\0';
		if (stopw.IsRunning()) {
			_tcscat(str, _T("Stopwatch: "));
			PrintElapsedTime(&str[_tcslen(str)],
				stopw.GetElapsedMs_i32(), false);	
		}
		if (timer.IsRunning()) {
			if (*str) { _tcscat(str, _T("\n")); }
			_tcscat(str, _T("Timer: "));
			PrintElapsedTime(&str[_tcslen(str)],
				timer_initial - timer.GetElapsedMs_i32(), false);
		}
		RECT rcTest, rcMini = {3, 3};
		SelectObject(hdc, hFontSmall);
		MyDrawText(hdc, str, &rcMini, DT_CALCRECT);
		if (!IntersectRect(&rcTest, &rcMini, &rcText)) {
			MyDrawText(hdc, str, &rcMini, 0);
		}
	}
	// Done
	BitBlt(real_hdc, 0, 0, WIDTH_OF(rc), HEIGHT_OF(rc), hdc, 0, 0, SRCCOPY);
	SelectObject(hdc, old_hbm);
	SelectObject(hdc, old_hfo);
	DeleteObject(hbm);
	DeleteDC(hdc);
	EndPaint(hWnd, &ps);
}

void OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags) {
	if (!fDown) { return; }
	if (vk == VK_ESCAPE) {
		ShowWindow(hWnd, SW_MINIMIZE);
		return;
	}
	if (vk == VK_TAB) { // Toggle screens
		if (HIBYTE(GetKeyState(VK_SHIFT))) { // holding SHIFT
			if (timer_mode) { stopw_mode = true; timer_mode = false; }
			else if (stopw_mode) { stopw_mode = timer_mode = false; }
			else { stopw_mode = false; timer_mode = true; }
		}
		else {
			if (stopw_mode) { stopw_mode = false; timer_mode = true; }
			else if (timer_mode) { stopw_mode = timer_mode = false; }
			else { stopw_mode = true; timer_mode = false; }
		}		
		return;
	}
	if (vk == VK_F12) {
		ShowSettingsDlg();
		return;
	}
	if (stopw_mode) {
		if (vk == VK_RETURN) {
			MessageBox(hWnd,
				_T("SPACE to start/pause, R to reset, L to lap.\n")
				_T("(Maximum 3 laps)\n\n")
				_T("Press BACK to return to Clock.\n")
				_T("Stopwatch will run in background.\n"),
				_T("Stopwatch"),
				MB_OK | MB_ICONINFORMATION);
		}
		else if (vk == VK_SPACE) { stopw_StartPause(); }
		else if (vk == 'R') { stopw.Reset(); }
		else if (vk == 'L') { stopw_Lap(); }
		else if (vk == VK_BACK) { stopw_mode = false; }
	}
	else if (timer_mode) {
		if (vk == VK_RETURN) {
			MessageBox(hWnd,
				_T("SPACE to start/pause, R to reset, Q to set timeout.\n\n")
				_T("Press BACK to return to Clock.\n")
				_T("Timer will run in background.\n"),
				_T("Timer"), MB_OK | MB_ICONINFORMATION);
		}
		else if (vk == VK_SPACE) { timer_StartPause(); }
		else if (vk == 'Q') { timer_SetTimeout(); }
		else if (vk == 'R') { timer_Reset(); }
		else if (vk == VK_BACK) { timer_mode = false; }
	}
	else /* clock mode */ {
		if (vk == VK_RETURN) {
			MessageBox(hWnd,
				_T("Press S for Stopwatch.\n")
				_T("Press T for Timer.\n")
				_T("Press TAB to toggle.\n")
				_T("Press F12 for Options.\n"),
				_T("Clock"),
				MB_OK | MB_ICONINFORMATION);
		}
		else if (vk == 'S') { stopw_mode = true; }
		else if (vk == 'T') { timer_mode = true; }
	}
}

void OnRButtonUp(HWND hWnd, int x, int y, UINT flags) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	if (stopw_mode) {
		AppendMenu(hMenu, MF_BYCOMMAND, 1, _T("Start/Pause"));
		AppendMenu(hMenu, MF_BYCOMMAND, 2, _T("Lap"));
		AppendMenu(hMenu, MF_BYCOMMAND, 3, _T("Reset"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_BYCOMMAND, 12, _T("Timer..."));
		AppendMenu(hMenu, MF_BYCOMMAND, 10, _T("Clock..."));
	}
	else if (timer_mode) {
		AppendMenu(hMenu, MF_BYCOMMAND, 1, _T("Start/Pause"));
		AppendMenu(hMenu, MF_BYCOMMAND, 2, _T("Set timeout..."));
		AppendMenu(hMenu, MF_BYCOMMAND, 3, _T("Reset"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_BYCOMMAND, 11, _T("Stopwatch..."));
		AppendMenu(hMenu, MF_BYCOMMAND, 10, _T("Clock..."));
	}
	else /* clock mode */ {
		AppendMenu(hMenu, MF_BYCOMMAND, 11, _T("Stopwatch..."));
		AppendMenu(hMenu, MF_BYCOMMAND, 12, _T("Timer..."));
	}
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_BYCOMMAND, 13, _T("Settings..."));
	AppendMenu(hMenu, MF_GRAYED, 0, 
		_T("temClock v1.1 by Raymai97"));
	int ret = TrackPopupMenu(
		hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hMenu);
	if (ret == 10) { stopw_mode = timer_mode = false; }
	else if (ret == 11) { stopw_mode = true; timer_mode = false; }
	else if (ret == 12) { stopw_mode = false; timer_mode = true; }
	else if (ret == 13) { ShowSettingsDlg(); }
	else if (stopw_mode) {
		if (ret == 1) { stopw_StartPause(); }
		else if (ret == 2) { stopw_Lap(); }
		else if (ret == 3) { stopw_Reset(); }
	}
	else if (timer_mode) {
		if (ret == 1) { timer_StartPause(); }
		else if (ret == 2) { timer_SetTimeout(); }
		else if (ret == 3) { timer_Reset(); }
	}	
}

UINT OnNcHitTest(HWND hwnd, int x, int y) {
	POINT pt = {x, y};
	ScreenToClient(hwnd, &pt);
	RECT rc = {0};
	GetClientRect(hwnd, &rc);
	if (PtInRect(&rc, pt)) { return HTCAPTION; }
	return FORWARD_WM_NCHITTEST(hwnd, x, y, DefWindowProc);
}

void stopw_StartPause() {
	if (stopw.IsRunning()) {
		if (stopw.IsPaused()) { stopw.Resume(); }
		else { stopw.Pause(); }
	}
	else { stopw.Start(); }
}

void stopw_Reset() {
	stopw.Reset();
	stopw_lap1 = stopw_lap2 = stopw_lap3 = 0;
}

void stopw_Lap() {
	if (!stopw_lap1) { stopw_lap1 = stopw.GetElapsedMs_i32(); }
	else if (!stopw_lap2) { stopw_lap2 = stopw.GetElapsedMs_i32(); }
	else if (!stopw_lap3) { stopw_lap3 = stopw.GetElapsedMs_i32(); }
	else {
		MessageBox(hWnd, _T("Maximum 3 laps only."), NULL,
			MB_OK | MB_ICONERROR);
	}
}

void timer_StartPause() {
	if (timer.IsRunning()) {
		if (timer.IsPaused()) {
			timer_id = SetTimer(NULL, NULL,
				timer_initial - timer.GetElapsedMs_i32(), OnTimerTimeout);
			timer.Resume();
		}
		else {
			KillTimer(NULL, timer_id);
			timer.Pause();
		}
	}
	else if (timer_initial) {
		timer_id = SetTimer(NULL, NULL, timer_initial, OnTimerTimeout);
		timer.Start();
	}
	else {
		MessageBox(hWnd, _T("Please set timeout first."),
			_T("Timer"), MB_OK | MB_ICONINFORMATION);
	}
}

void timer_Reset() {
	timer.Reset();
	KillTimer(NULL, timer_id);
}

void timer_SetTimeout() {
	if (timer.IsRunning()) {
		if (IDYES == MessageBox(hWnd,
			_T("You're trying to set timeout.\n")
			_T("If you continue, the current timer will be reset.\n")
			_T("Are you sure?"),
			_T("Sure?"),
			MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION))
		{
			timer_Reset();
		}
		else { return; }
	}
	DialogBox(hInst, MAKEINTRESOURCE(DLG_TIMESPAN), hWnd, TimespanDlgProc);
}

void CenterParent(HWND hChild, HWND hParent) {
	RECT rc, rc2;
	GetWindowRect(hParent, &rc);
	GetWindowRect(hChild, &rc2);
	SetWindowPos(hChild, HWND_TOP,
		rc.left + (WIDTH_OF(rc) - WIDTH_OF(rc2)) / 2,
		rc.top + (HEIGHT_OF(rc) - HEIGHT_OF(rc2)) / 2,
		0, 0, SWP_NOSIZE);
}

void MsToMulti(i32_t total_ms, i32_t *h, i32_t *m, i32_t *s, i32_t *ms) {
	*ms = total_ms;
	*h = *ms / 60 / 60 / 1000;
	*ms -= (*h * 60 * 60 * 1000);
	*m = *ms / 60 / 1000;
	*ms -= (*m * 60 * 1000);
	*s = *ms / 1000;
	*ms -= (*s * 1000);
}

void PrintElapsedTime(TCHAR *str, i32_t total_ms, bool show_ms) {
	if (total_ms < 0) {
		_stprintf(str, _T("Overflow..."));
		return;
	}
	i32_t h = 0, m = 0, s = 0, ms = 0;
	MsToMulti(total_ms, &h, &m, &s, &ms);
	if (show_ms) {
		_stprintf(str, _T("%.2i:%.2i:%.2i.%.3i"),
			h, m, s, ms);
	}
	else {
		_stprintf(str, _T("%.2i:%.2i:%.2i"),
			h, m, s);
	}
}

// Behaves like .NET TryParse, but for user convenience,
// it will assume 'zero-length input' as '0'.
bool TryParseTextOf(HWND hDlg, int ctlId, i32_t *pDest) {
	HWND h = GetDlgItem(hDlg, ctlId);
	if (GetWindowTextLength(h) == 0) {
		*pDest = 0;
		return true;
	}
	TCHAR buf[20] = {0}, *pEnd = NULL;
	GetWindowText(h, buf, 20);
	i32_t ret = _tcstol(buf, &pEnd, 10);
	if (pEnd && *pEnd) { return false; }
	if (ret >= I32_MAX) { return false; }
	*pDest = ret;
	return true;
}

bool TryAdd(i32_t n, i32_t *p) {
	if ((n > 0) && (*p > I32_MAX - n)) { return false; } // overflow
	if ((n < 0) && (*p < I32_MIN - n)) { return false; } // underflow
	*p += n;
	return true;
}

bool TryMultiply(i32_t n, i32_t *p) {
	if (*p > I32_MAX / n) { return false; } // overflow
	if (*p < I32_MIN / n) { return false; } // underflow
	// try think 8-bit signed,
	// what would happen if -256 * -1
	if ((*p == -1) && ( n == I32_MIN)) { return false; }
	if (( n == -1) && (*p == I32_MIN)) { return false; }
	*p *= n;
	return true;
}

void ShowSettingsDlg() {
	// Backup and copy to make sure completely undo-able
	dlgSettings_lfFont = lfFont;
	dlgSettings_lfFontSmall = lfFontSmall;
	// Keep change if return TRUE
	if (DialogBox(hInst, MAKEINTRESOURCE(DLG_SETTINGS), hWnd, SettingsDlgProc)) {
		lfFont = dlgSettings_lfFont;
		lfFontSmall = dlgSettings_lfFontSmall;
	}
	else { // revert change
		hFont = CreateFontIndirect(&lfFont);
		hFontSmall = CreateFontIndirect(&lfFontSmall);
		dwmf.DoExtend(dwm_extend_frame);
	}
}

void LoadSaveIni(HWND hDlg, bool save) {
	BOOL ok;
	TCHAR szMsg[STR_SIZE] = {0};
	if (save) {
#ifdef _UNICODE
		// If INI doesn't exist, we must manually create a blank
		// Unicode (UTF16-LE) text file, or it will be ANSI encoding.
		FILE *file = _tfopen(ini_filepath, _T("w,ccs=UNICODE"));
		if (file) {
			WORD utf16_le_bom = 0xFEFF;
			fwrite(&utf16_le_bom, sizeof(WORD), 1, file);
			fclose(file);
		}
#endif
		TCHAR szExec[MAX_PATH] = {0}, szSound[MAX_PATH] = {0};
		if (IsDlgButtonChecked(hDlg, CHK_EXECUTE)) {
			GetDlgItemText(hDlg, TXT_EXECUTE, szExec, MAX_PATH);
		}
		if (IsDlgButtonChecked(hDlg, CHK_PLAY_SOUND)) {
			GetDlgItemText(hDlg, TXT_SOUND, szSound, MAX_PATH);
		}
		ok =
		WritePrivateProfileStruct(
			_T("TemClock"), _T("Font"), &dlgSettings_lfFont,
			sizeof(LOGFONT), ini_filepath) &&
		WritePrivateProfileStruct(
			_T("TemClock"), _T("MiniFont"), &dlgSettings_lfFontSmall,
			sizeof(LOGFONT), ini_filepath) &&
		WritePrivateProfileString(
			_T("TemClock"), _T("DwmExtendFrame"),
			IsDlgButtonChecked(hDlg, CHK_DWM_EXTEND_FRAME)
			? _T("1") : _T("0"), ini_filepath) &&
		WritePrivateProfileString(
			_T("OnTimeout"), _T("Execute"), szExec, ini_filepath) &&
		WritePrivateProfileString(
			_T("OnTimeout"), _T("Sound"), szSound, ini_filepath);
		if (!ok) { _tcscpy(szMsg, _T("Failed to write INI file.")); }
	}
	else {
		LOGFONT lf1, lf2;
		HFONT hfo1, hfo2;
		ok =
		GetPrivateProfileStruct(
			_T("TemClock"), _T("Font"), &lf1, sizeof(LOGFONT), ini_filepath) &&
		GetPrivateProfileStruct(
			_T("TemClock"), _T("MiniFont"), &lf2, sizeof(LOGFONT), ini_filepath) &&
		(hfo1 = CreateFontIndirect(&lf1)) &&
		(hfo2 = CreateFontIndirect(&lf2));
		if (!ok) { _tcscpy(szMsg, _T("Failed to load font.")); }
		else {
			DeleteObject(hFont);
			DeleteObject(hFontSmall);
			hFont = hfo1;
			hFontSmall = hfo2;
			dlgSettings_lfFont = lf1;
			dlgSettings_lfFontSmall = lf2;
			// Error handling omitted (too tedious to do so)
			TCHAR buf[MAX_PATH] = {0};
			GetPrivateProfileString(
				_T("TemClock"), _T("DwmExtendFrame"), _T(""),
				buf, MAX_PATH, ini_filepath);
			dwm_extend_frame = (_tcscmp(buf, _T("1")) == 0);
			GetPrivateProfileString(
				_T("OnTimeout"), _T("Execute"), _T(""),
				ontimeout_exec, MAX_PATH, ini_filepath);
			GetPrivateProfileString(
				_T("OnTimeout"), _T("Sound"),  _T(""),
				ontimeout_sound, MAX_PATH, ini_filepath);
			dlgSettings_UpdateCtrl(hDlg);
			dlgSettings_OnCheckBoxToggled(hDlg);
		}
	}
	if (ok) {
		_stprintf(szMsg, _T("Settings have been %s %s."),
			save ? _T("saved to") : _T("loaded from"), INI_FILENAME);
	}
	else {
		DWORD err_code = GetLastError();
		TCHAR *err_msg;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			err_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&err_msg, 0, NULL);
		_stprintf(&szMsg[_tcslen(szMsg)],
			_T("\n%s(Error code: %lu)"), err_msg, err_code);
	}
	MessageBox(hDlg, szMsg, ok ? (save ? _T("Saved") : _T("Loaded")) : NULL,
		MB_OK | ok ? MB_ICONINFORMATION : MB_ICONERROR);
}

void MyDrawText(HDC hdcMem, LPTSTR szText, LPRECT prc, UINT dtFlags) {
	if (!(dtFlags & DT_CALCRECT) && dwmf.IsExtended()) {
#ifdef UNICODE
		LPCWSTR wszText = szText;
#else
		WCHAR wszText[STR_SIZE];
		MultiByteToWideChar(CP_UTF8, 0, szText, -1, wszText, STR_SIZE);
#endif
		UxtDrawTextOnDwmFrameOffscreen(
			hdcMem, wszText,
			prc->left, prc->top,
			WIDTH_OF(*prc), HEIGHT_OF(*prc),
			dtFlags, 10, RGB(0,0,0));
	}
	else {
		DrawText(hdcMem, szText, -1, prc, dtFlags);
	}
}

// Timespan dialog ...

BOOL CALLBACK TimespanDlgProc(HWND hDlg, UINT msg, WPARAM w, LPARAM l) {
	switch (msg) {
	case WM_COMMAND: return HANDLE_WM_COMMAND(hDlg, w, l, dlgTimespan_OnCommand);
	case WM_INITDIALOG: return HANDLE_WM_INITDIALOG(hDlg, w, l, dlgTimespan_OnInitDialog);
	}
	return FALSE; // return FALSE let system do the default stuff.
}

BOOL dlgTimespan_OnInitDialog(HWND hDlg, HWND, LPARAM) {
	CenterParent(hDlg, hWnd);
	// Set default value
	i32_t h = 0, m = 0, s = 0, ms = 0;
	MsToMulti(timer_initial, &h, &m, &s, &ms);
	HWND h_txtH = GetDlgItem(hDlg, TXT_H);
	HWND h_txtM = GetDlgItem(hDlg, TXT_M);
	HWND h_txtS = GetDlgItem(hDlg, TXT_S);
	HWND h_txtMs = GetDlgItem(hDlg, TXT_MS);
	TCHAR buf[40] = {0};
	SetWindowText(h_txtH, _ultot(h, buf, 10));
	SetWindowText(h_txtM, _ultot(m, buf, 10));
	SetWindowText(h_txtS, _ultot(s, buf, 10));
	SetWindowText(h_txtMs, _ultot(ms, buf, 10));
	// Focus and Select All
	SetFocus(h_txtH);
	SendMessage(h_txtH, EM_SETSEL, 0, -1);
	return FALSE;
}

BOOL dlgTimespan_OnCommand(HWND hDlg, int id, HWND hCtl, UINT) {
	switch (id) {
	case IDOK: dlgTimespan_OnBtnOK(hDlg); break;
	case IDCANCEL: EndDialog(hDlg, FALSE); break;
	}
	return TRUE;
}

void dlgTimespan_OnBtnOK(HWND hDlg) {
	i32_t h = 0, m = 0, s = 0, ms = 0;
	LPTSTR szErrMsg = 
		!TryParseTextOf(hDlg, TXT_H, &h) ||
		!TryParseTextOf(hDlg, TXT_M, &m) ||
		!TryParseTextOf(hDlg, TXT_S, &s) ||
		!TryParseTextOf(hDlg, TXT_MS, &ms) ?
		_T("Parse integer failed!") :
		h < 0 || m < 0 || s < 0 || ms < 0 ?
		_T("Please enter positive number only!") :
		!TryMultiply(60 * 60 * 1000, &h) ||
		!TryMultiply(60 * 1000, &m) ||
		!TryMultiply(1000, &s) ||
		!TryAdd(s, &ms) || !TryAdd(m, &ms) || !TryAdd(h, &ms) ?
		_T("Integer overflow! Please enter smaller value.") : NULL;
	if (szErrMsg) {
		MessageBox(hDlg, szErrMsg, NULL, MB_OK | MB_ICONERROR);
		return;
	}
	timer_initial = ms;
	EndDialog(hDlg, TRUE);
}

// Settings dialog ...

BOOL CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM w, LPARAM l) {
	switch (msg) {
	case WM_COMMAND: return HANDLE_WM_COMMAND(hDlg, w, l, dlgSettings_OnCommand);
	case WM_INITDIALOG: return HANDLE_WM_INITDIALOG(hDlg, w, l, dlgSettings_OnInitDialog);

	}
	return FALSE; // return FALSE let system do the default stuff.
}

BOOL dlgSettings_OnInitDialog(HWND hDlg, HWND, LPARAM) {
	CenterParent(hDlg, hWnd);
	SetWindowFont(GetDlgItem(hDlg, BTN_TEST_SOUND),
		dlgSettings_hFontWebdings, TRUE);
	dlgSettings_UpdateCtrl(hDlg);
	dlgSettings_OnCheckBoxToggled(hDlg);
	SetFocus(GetDlgItem(hDlg, IDOK));
	return FALSE;
}

void dlgSettings_UpdateCtrl(HWND hDlg) {
	SetDlgItemText(hDlg, TXT_EXECUTE, ontimeout_exec);
	SetDlgItemText(hDlg, TXT_SOUND, ontimeout_sound);
	CheckDlgButton(hDlg, CHK_EXECUTE, *ontimeout_exec ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, CHK_PLAY_SOUND, *ontimeout_sound ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, CHK_DWM_EXTEND_FRAME, dwm_extend_frame ? BST_CHECKED : BST_UNCHECKED);
}

void dlgSettings_OnCheckBoxToggled(HWND hDlg) {
	BOOL do_exec = IsDlgButtonChecked(hDlg, CHK_EXECUTE);
	BOOL do_play_sound = IsDlgButtonChecked(hDlg, CHK_PLAY_SOUND);
	HWND h_txtExec = GetDlgItem(hDlg, TXT_EXECUTE);
	HWND h_btnBrowseExe = GetDlgItem(hDlg, BTN_BROWSE_EXE);	
	HWND h_txtSound = GetDlgItem(hDlg, TXT_SOUND);
	HWND h_btnBrowseSound = GetDlgItem(hDlg, BTN_BROWSE_SOUND);
	HWND h_btnTestSound = GetDlgItem(hDlg, BTN_TEST_SOUND);
	EnableWindow(h_txtExec, do_exec);
	EnableWindow(h_btnBrowseExe, do_exec);
	EnableWindow(h_txtSound, do_play_sound);
	EnableWindow(h_btnBrowseSound, do_play_sound);
	EnableWindow(h_btnTestSound, do_play_sound);
	dwmf.DoExtend(BST_CHECKED ==
		IsDlgButtonChecked(hDlg, CHK_DWM_EXTEND_FRAME));
}

BOOL dlgSettings_OnCommand(HWND hDlg, int id, HWND hCtl, UINT notify_code) {
	switch (id) {
	case IDOK: dlgSettings_OnBtnOK(hDlg); break;
	case IDCANCEL: EndDialog(hDlg, FALSE); break;
	case BTN_SET_FONT: dlgSettings_OnBtnSetFont(hDlg); break;
	case BTN_SET_MINI_FONT: dlgSettings_OnBtnSetMiniFont(hDlg); break;
	case CHK_EXECUTE:
	case CHK_PLAY_SOUND: 
	case CHK_DWM_EXTEND_FRAME: dlgSettings_OnCheckBoxToggled(hDlg); break;
	case BTN_BROWSE_EXE: dlgSettings_OnBtnBrowseExe(hDlg); break;
	case BTN_BROWSE_SOUND: dlgSettings_OnBtnBrowseSound(hDlg); break;
	case BTN_TEST_SOUND: dlgSettings_OnBtnTestSound(hDlg); break;
	case BTN_SAVE_INI: dlgSettings_OnBtnSaveINI(hDlg); break;
	case BTN_LOAD_INI: dlgSettings_OnBtnLoadINI(hDlg); break;
	}
	return TRUE;
}

void dlgSettings_OnBtnOK(HWND hDlg) {
	BOOL do_exec = IsDlgButtonChecked(hDlg, CHK_EXECUTE);
	BOOL do_play_sound = IsDlgButtonChecked(hDlg, CHK_PLAY_SOUND);
	if (do_exec) { GetDlgItemText(hDlg, TXT_EXECUTE, ontimeout_exec, MAX_PATH); }
	else { *ontimeout_exec = '\0'; }
	if (do_play_sound) { GetDlgItemText(hDlg, TXT_SOUND, ontimeout_sound, MAX_PATH); }
	else { *ontimeout_sound = '\0'; }
	dwm_extend_frame = BST_CHECKED ==
		IsDlgButtonChecked(hDlg, CHK_DWM_EXTEND_FRAME);
	EndDialog(hDlg, TRUE);
}

void dlgSettings_OnBtnSaveINI(HWND hDlg) {
	LoadSaveIni(hDlg, true);	
}

void dlgSettings_OnBtnLoadINI(HWND hDlg) {
	LoadSaveIni(hDlg, false);
}

// On dlgSettings_Init...
// lfFont and lfFontSmall are copied to dlgSettings_lf...
// If pressed cancel, CreateFont using lfFont and lfFontSmall
void dlgSettings_OnBtnSetFont(HWND hDlg) {
	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = hDlg;
	cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST;
	cf.lpLogFont = &dlgSettings_lfFont;
	if (ChooseFont(&cf)) {
		DeleteObject(hFont);
		hFont = CreateFontIndirect(&dlgSettings_lfFont);
	}
}
void dlgSettings_OnBtnSetMiniFont(HWND hDlg) {
	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = hDlg;
	cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST;
	cf.lpLogFont = &dlgSettings_lfFontSmall;
	if (ChooseFont(&cf)) {
		DeleteObject(hFontSmall);
		hFontSmall = CreateFontIndirect(&dlgSettings_lfFontSmall);
	}
}
void dlgSettings_OnBtnBrowseExe(HWND hDlg) {
	TCHAR buf[MAX_PATH] = {0};
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	ofn.hwndOwner = hDlg;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = _T("Executable files\0") _T("*.exe\0");
	if (GetOpenFileName(&ofn)) {
		SetDlgItemText(hDlg, TXT_EXECUTE, buf);
	}
}
void dlgSettings_OnBtnBrowseSound(HWND hDlg) {
	TCHAR buf[MAX_PATH] = {0};
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	ofn.hwndOwner = hDlg;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = _T("WAV files\0") _T("*.wav\0");
	if (GetOpenFileName(&ofn)) {
		SetDlgItemText(hDlg, TXT_SOUND, buf);
	}
}
void dlgSettings_OnBtnTestSound(HWND hDlg) {
	TCHAR szSound[MAX_PATH] = {0};
	GetDlgItemText(hDlg, TXT_SOUND, szSound, MAX_PATH);
	PlaySound(szSound, NULL,
		SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
}
