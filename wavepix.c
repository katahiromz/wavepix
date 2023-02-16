#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <dlgs.h>
#include <tchar.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "wavepix.h"
#include "resource.h"

static const TCHAR g_szWndClass[] = TEXT("WavePix Wnd Class");
static const TCHAR g_szCanvasWndClass[] = TEXT("WavePix Canvas Wnd Class");

HINSTANCE g_hInstance;
HWND g_hMainWnd;
HWND g_hCanvasWnd;
HWND g_hSetttingDlg;
HACCEL g_hAccel;

TCHAR g_szFileName[MAX_PATH] = TEXT("");
INT g_iImageType = BMP;
TCHAR g_szInputFilter[1024];
TCHAR g_szOutputFilter[1024];

double g_eAmplitude, g_eWaveLength, g_ePhase;
BOOL g_fPercent1;
BOOL g_fPercent2;
BOOL g_fVertical = FALSE;

BOOL g_fChanging;

SIZE g_sizOriginal;
HBITMAP g_hbmOriginal = NULL;
SIZE g_sizWaved;
HBITMAP g_hbmWaved = NULL;
COLORREF g_clrBack = CLR_INVALID;

INT g_nZoomPercent = 100;
INT g_xScrollPos = -1;
INT g_yScrollPos = -1;

COLORREF g_clrTransparent = CLR_INVALID;

float g_dpi;  /* dots per inch */

LPCTSTR LoadStringDx(INT id)
{
    static TCHAR sz[1024];
    LoadString(g_hInstance, id, sz, 1024);
    return sz;
}

double GetDlgItemDouble(HWND hDlg, INT id)
{
    TCHAR sz[128];
    GetDlgItemText(hDlg, id, sz, 128);
    return _ttof(sz);
}

void SetDlgItemDouble(HWND hDlg, INT id, double d)
{
    TCHAR sz[128];
    static const TCHAR format[] = TEXT("%.2lf");
    _stprintf(sz, format, d);
    SetDlgItemText(hDlg, id, sz);
}

void RemoveExtension(LPTSTR pszFileName)
{
    LPTSTR pch;
    pch = _tcsrchr(pszFileName, '\\');
    if (pch == NULL)
        return;
    pch = strrchr(pch, '.');
    if (pch == NULL)
        return;
    *pch = 0;
}

INT GetImageType(LPCTSTR pszFileName)
{
    LPCTSTR pch;
    pch = _tcsrchr(pszFileName, '.');
    if (pch == NULL)
        return 0;
    if (lstrcmpi(pch, ".bmp") == 0)
        return BMP;
    if (lstrcmpi(pch, ".gif") == 0)
        return GIF;
    if (lstrcmpi(pch, ".jpg") == 0 || lstrcmpi(pch, ".jpe") == 0 ||
        lstrcmpi(pch, ".jpeg") == 0 || lstrcmpi(pch, ".jfif") == 0)
        return JPEG;
    if (lstrcmpi(pch, ".png") == 0)
        return PNG;
    if (lstrcmpi(pch, ".tif") == 0 || lstrcmpi(pch, ".tiff") == 0)
        return TIFF;
    return 0;
}

VOID UpdateWindowCaption(VOID)
{
    TCHAR szFileTitle[MAX_PATH];
    TCHAR szCaption[1024];

    if (g_szFileName[0] != 0)
    {
        GetFileTitle(g_szFileName, szFileTitle, MAX_PATH);
        wsprintf(szCaption, LoadStringDx(2), szFileTitle);
    }
    else
    {
        lstrcpy(szCaption, LoadStringDx(1));
    }
    SetWindowText(g_hMainWnd, szCaption);
}

VOID SetFileName(LPCTSTR pszFileName)
{
    lstrcpyn(g_szFileName, pszFileName, MAX_PATH);
}

HBITMAP CreateBackBitmap(void)
{
    HBITMAP hbm;
    HDC hdc;
    BITMAPINFO bi;
    VOID *pvBits;

    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_sizWaved.cx;
    bi.bmiHeader.biHeight = g_sizWaved.cy;
    bi.bmiHeader.biPlanes = 1;
    if (g_clrBack == CLR_INVALID)
        bi.bmiHeader.biBitCount = 32;
    else
        bi.bmiHeader.biBitCount = 24;
    hdc = CreateCompatibleDC(NULL);
    if (hdc != NULL)
    {
        hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        if (hbm != NULL && g_clrBack != CLR_INVALID)
        {
            RECT rc;
            HBRUSH hbr = CreateSolidBrush(g_clrBack);
            HGDIOBJ hbmOld = SelectObject(hdc, hbm);
            HGDIOBJ hbrOld = SelectObject(hdc, hbr);
            SetRect(&rc, 0, 0, g_sizWaved.cx, g_sizWaved.cy);
            FillRect(hdc, &rc, hbr);
            SelectObject(hdc, hbrOld);
            SelectObject(hdc, hbmOld);
            DeleteObject(hbr);
        }
        DeleteDC(hdc);
    }
    if (g_clrBack == CLR_INVALID)
    {
        INT x, y;
        LPDWORD pdw = (LPDWORD)pvBits;
        for(y = 0; y < bi.bmiHeader.biHeight; y++)
        {
            for(x = 0; x < bi.bmiHeader.biWidth; x++)
            {
                *pdw++ = ((x >> 3) & 1) ^ ((y >> 3) & 1) ? 0xFFC0C0C0 : 0xFFFFFFFF;
            }
        }
    }
    return hbm;
}

void Canvas_OnPaint(HWND hWnd, HDC hdc, RECT *prcPaint)
{
    RECT rc;
    SIZE siz;
    INT x, y;
    HBITMAP hbm;
    HDC hdc3;
    HGDIOBJ hbm3Old;

    if (g_hbmOriginal == NULL || g_hbmWaved == NULL)
        return;

    hbm = CreateBackBitmap();
    if (hbm != NULL)
    {
        GetClientRect(hWnd, &rc);
        siz.cx = rc.right - rc.left;
        siz.cy = rc.bottom - rc.top;
        AlphaBlendBitmap(hbm, g_hbmWaved);

        if (g_xScrollPos == -1)
            x = (siz.cx - g_sizWaved.cx * g_nZoomPercent / 100) / 2;
        else
            x = -g_xScrollPos;
        if (g_yScrollPos == -1)
            y = (siz.cy - g_sizWaved.cy * g_nZoomPercent / 100) / 2;
        else
            y = -g_yScrollPos;

        hdc3 = CreateCompatibleDC(NULL);
        hbm3Old = SelectObject(hdc3, hbm);
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchBlt(hdc,
            x,
            y,
            g_sizWaved.cx * g_nZoomPercent / 100,
            g_sizWaved.cy * g_nZoomPercent / 100,
            hdc3,
            0,
            0,
            g_sizWaved.cx,
            g_sizWaved.cy,
            SRCCOPY);
        SelectObject(hdc3, hbm3Old);
        DeleteDC(hdc3);
        DeleteObject(hbm);
    }
}

void Canvas_OnSize(HWND hWnd)
{
    RECT rc;
    SIZE siz, siz2;
    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    siz2.cx = g_sizWaved.cx * g_nZoomPercent / 100;
    siz2.cy = g_sizWaved.cy * g_nZoomPercent / 100;
    if (siz.cx < siz2.cx)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = siz2.cx;
        si.nPage = siz.cx;
        g_xScrollPos = si.nPos = (siz2.cx - siz.cx) / 2;
        SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);
        ShowScrollBar(hWnd, SB_HORZ, TRUE);
    }
    else
    {
        g_xScrollPos = -1;
        ShowScrollBar(hWnd, SB_HORZ, FALSE);
    }
    if (siz.cy < siz2.cy)
    {
        SCROLLINFO si;
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = siz2.cy;
        si.nPage = siz.cy;
        g_yScrollPos = si.nPos = (siz2.cy - siz.cy) / 2;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        ShowScrollBar(hWnd, SB_VERT, TRUE);
    }
    else
    {
        g_yScrollPos = -1;
        ShowScrollBar(hWnd, SB_VERT, FALSE);
    }
    InvalidateRect(hWnd, NULL, TRUE);
}

void OnHScroll(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    SIZE siz, siz2;
    INT nMin, nMax, nPos, nDelta;
    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    GetScrollRange(hWnd, SB_HORZ, &nMin, &nMax);
    nPos = GetScrollPos(hWnd, SB_HORZ);
    switch(LOWORD(wParam))
    {
    case SB_LINELEFT:
        nPos -= 10;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_LINERIGHT:
        nPos += 10;
        if (nMax - siz.cx + 1 < nPos)
            nPos = nMax - siz.cx + 1;
        break;

    case SB_PAGELEFT:
        nPos -= siz.cx;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_PAGERIGHT:
        nPos += siz.cx;
        if (nMax - siz.cx + 1 < nPos)
            nPos = nMax - siz.cx + 1;
        break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        nPos = (SHORT)HIWORD(wParam);
        break;
    }
    nDelta = nPos - g_xScrollPos;
    if (abs(nDelta) < siz.cx)
        ScrollWindow(hWnd, -nDelta, 0, &rc, NULL);
    if (nDelta > 0)
    {
        rc.left = rc.right - nDelta;
    }
    else
    {
        rc.right = rc.left - nDelta;
    }
    g_xScrollPos = nPos;
    SetScrollPos(hWnd, SB_HORZ, nPos, TRUE);
    if (abs(nDelta) < siz.cx)
        InvalidateRect(hWnd, &rc, TRUE);
    else
        InvalidateRect(hWnd, NULL, TRUE);
}

void OnVScroll(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    SIZE siz, siz2;
    INT nMin, nMax, nPos, nDelta;
    GetClientRect(hWnd, &rc);
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    GetScrollRange(hWnd, SB_VERT, &nMin, &nMax);
    nPos = GetScrollPos(hWnd, SB_VERT);
    switch(LOWORD(wParam))
    {
    case SB_LINEUP:
        nPos -= 10;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_LINEDOWN:
        nPos += 10;
        if (nMax - siz.cy + 1 < nPos)
            nPos = nMax - siz.cy + 1;
        break;

    case SB_PAGEUP:
        nPos -= siz.cy;
        if (nPos < nMin)
            nPos = nMin;
        break;

    case SB_PAGEDOWN:
        nPos += siz.cy;
        if (nMax - siz.cy + 1 < nPos)
            nPos = nMax - siz.cy + 1;
        break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        nPos = (SHORT)HIWORD(wParam);
        break;
    }
    nDelta = nPos - g_yScrollPos;
    if (abs(nDelta) < siz.cy)
        ScrollWindow(hWnd, 0, -nDelta, &rc, NULL);
    if (nDelta > 0)
    {
        rc.top = rc.bottom - nDelta;
    }
    else
    {
        rc.bottom = rc.top - nDelta;
    }
    g_yScrollPos = nPos;
    SetScrollPos(hWnd, SB_VERT, nPos, TRUE);
    if (abs(nDelta) < siz.cy)
        InvalidateRect(hWnd, &rc, TRUE);
    else
        InvalidateRect(hWnd, NULL, TRUE);
}

VOID GetWaveSetting(HWND hDlg)
{
    INT i;
    g_fVertical = (IsDlgButtonChecked(hDlg, IDVERTICAL) == BST_CHECKED);
    i = SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_GETCURSEL, 0, 0);
    g_fPercent1 = (i == 0);
    i = SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_GETCURSEL, 0, 0);
    g_fPercent2 = (i == 0);

    g_eAmplitude = GetDlgItemDouble(hDlg, IDAMPLITUDE);
    g_eWaveLength = GetDlgItemDouble(hDlg, IDWAVELENGTH);
    g_ePhase = GetDlgItemDouble(hDlg, IDPHASE) * M_PI / 180.0;
    if (g_fVertical)
    {
        if (g_fPercent1)
            g_eAmplitude *= (double)g_sizOriginal.cx / 100.0;
        if (g_fPercent2)
            g_eWaveLength *= (double)g_sizOriginal.cy / 100.0;
    }
    else
    {
        if (g_fPercent1)
            g_eAmplitude *= (double)g_sizOriginal.cy / 100.0;
        if (g_fPercent2)
            g_eWaveLength *= (double)g_sizOriginal.cx / 100.0;
    }
    if (g_eWaveLength == 0.0)
    {
        if (g_fVertical)
            g_eWaveLength = (double)g_sizOriginal.cy;
        else
            g_eWaveLength = (double)g_sizOriginal.cx;
    }
}

VOID SetWaveSetting(HWND hDlg)
{
    g_fChanging = TRUE;
    if (g_fVertical)
    {
        CheckRadioButton(hDlg, IDHORIZONTAL, IDVERTICAL, IDVERTICAL);
        if (g_fPercent1)
        {
            SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_SETCURSEL, 0, 0);
            SetDlgItemDouble(hDlg, IDAMPLITUDE, g_eAmplitude * 100.0 / g_sizOriginal.cx);
        }
        else
        {
            SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_SETCURSEL, 1, 0);
            SetDlgItemDouble(hDlg, IDAMPLITUDE, g_eAmplitude);
        }
        if (g_fPercent2)
        {
            SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_SETCURSEL, 0, 0);
            SetDlgItemDouble(hDlg, IDWAVELENGTH, g_eWaveLength * 100.0 / g_sizOriginal.cy);
        }
        else
        {
            SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_SETCURSEL, 1, 0);
            SetDlgItemDouble(hDlg, IDWAVELENGTH, g_eWaveLength);
        }
    }
    else
    {
        CheckRadioButton(hDlg, IDHORIZONTAL, IDVERTICAL, IDHORIZONTAL);
        if (g_fPercent1)
        {
            SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_SETCURSEL, 0, 0);
            SetDlgItemDouble(hDlg, IDAMPLITUDE, g_eAmplitude * 100.0 / g_sizOriginal.cy);
        }
        else
        {
            SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_SETCURSEL, 1, 0);
            SetDlgItemDouble(hDlg, IDAMPLITUDE, g_eAmplitude);
        }
        if (g_fPercent2)
        {
            SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_SETCURSEL, 0, 0);
            SetDlgItemDouble(hDlg, IDWAVELENGTH, g_eWaveLength * 100.0 / g_sizOriginal.cx);
        }
        else
        {
            SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_SETCURSEL, 1, 0);
            SetDlgItemDouble(hDlg, IDWAVELENGTH, g_eWaveLength);
        }
    }
    SetDlgItemDouble(hDlg, IDPHASE, g_ePhase * 180.0 / M_PI);
    g_fChanging = FALSE;
}

void OnChange(void)
{
    HBITMAP hbm;
    HCURSOR hcurWait, hcurOld;

    if (g_hbmOriginal == NULL)
        return;

    hcurWait = LoadCursor(NULL, IDC_WAIT);
    hcurOld = SetCursor(hcurWait);
    hbm = CreateWaved32BppBitmap(g_hbmOriginal, g_eAmplitude, g_eWaveLength, g_ePhase, g_fVertical);
    if (hbm != NULL)
    {
        BITMAP bm;
        if (g_hbmWaved != NULL)
            DeleteObject(g_hbmWaved);
        g_hbmWaved = hbm;
        GetObject(hbm, sizeof(BITMAP), &bm);
        g_sizWaved.cx = bm.bmWidth;
        g_sizWaved.cy = bm.bmHeight;
        Canvas_OnSize(g_hCanvasWnd);
        InvalidateRect(g_hCanvasWnd, NULL, TRUE);
    }
    SetCursor(hcurOld);
}

BOOL CALLBACK
SettingDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    INT i;
    switch(uMsg)
    {
    case WM_INITDIALOG:
        g_fChanging = FALSE;
        SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStringDx(24));
        SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStringDx(25));
        SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_SETCURSEL, 0, 0);
        SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStringDx(24));
        SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStringDx(25));
        SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_SETCURSEL, 0, 0);
        SetWaveSetting(hDlg);
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDAMPLITUDE:
        case IDWAVELENGTH:
        case IDPHASE:
            if (!g_fChanging)
            {
                if (HIWORD(wParam) == EN_CHANGE)
                {
                    GetWaveSetting(hDlg);
                    OnChange();
                }
                else if (HIWORD(wParam) == EN_KILLFOCUS)
                {
                    GetWaveSetting(hDlg);
                    SetWaveSetting(hDlg);
                    OnChange();
                }
            }
            break;

        case IDHORIZONTAL:
        case IDVERTICAL:
            if (!g_fChanging)
            {
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    GetWaveSetting(hDlg);
                    SetWaveSetting(hDlg);
                    OnChange();
                }
            }
            break;

        case IDAMPLITUDE_COMBO:
            if (HIWORD(wParam) == CBN_SELENDOK)
            {
                i = SendDlgItemMessage(hDlg, IDAMPLITUDE_COMBO, CB_GETCURSEL, 0, 0);
                g_fPercent1 = (i == 0);
                SetWaveSetting(hDlg);
                OnChange();
            }
            break;

        case IDWAVELENGTH_COMBO:
            if (HIWORD(wParam) == CBN_SELENDOK)
            {
                i = SendDlgItemMessage(hDlg, IDWAVELENGTH_COMBO, CB_GETCURSEL, 0, 0);
                g_fPercent2 = (i == 0);
                SetWaveSetting(hDlg);
                OnChange();
            }
            break;

        case IDOK:
            GetWaveSetting(hDlg);
            SetWaveSetting(hDlg);
            OnChange();
            break;

        //case IDCANCEL:
        //    ShowWindow(hDlg, SW_HIDE);
        //    OnChange();
        //    break;
        }
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            ShowWindow(hDlg, SW_HIDE);
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK
CanvasWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    PAINTSTRUCT ps;
    HDC hdc;
    switch(uMsg)
    {
    case WM_SIZE:
        Canvas_OnSize(hWnd);
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        if (hdc != NULL)
        {
            Canvas_OnPaint(hWnd, hdc, &ps.rcPaint);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_HSCROLL:
        OnHScroll(hWnd, wParam, lParam);
        break;

    case WM_VSCROLL:
        OnVScroll(hWnd, wParam, lParam);
        break;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

BOOL DoOpenFile(LPCTSTR pszFileName)
{
    HBITMAP hbm;
    float dpi;
    HCURSOR hcurWait = LoadCursor(NULL, IDC_WAIT);
    HCURSOR hcurOld = SetCursor(hcurWait);
    INT i = GetImageType(pszFileName);
    switch(i)
    {
    case BMP:
        hbm = LoadBitmapFromFile(pszFileName, &dpi);
        break;

    case GIF:
        dpi = 0.0;
        hbm = LoadGifAsBitmap(pszFileName, &g_clrTransparent);
        break;

    case JPEG:
        hbm = LoadJpegAsBitmap(pszFileName, &dpi);
        break;

    case PNG:
        hbm = LoadPngAsBitmap(pszFileName, &dpi);
        break;

    case TIFF:
        hbm = LoadTiffAsBitmap(pszFileName, &dpi);
        break;
    }

    if (hbm != NULL)
    {
        BITMAP bm;
        g_dpi = dpi;
        if (g_hbmOriginal != NULL)
            DeleteObject(g_hbmOriginal);
        g_hbmOriginal = hbm;
        GetObject(hbm, sizeof(BITMAP), &bm);
        g_sizOriginal.cx = bm.bmWidth;
        g_sizOriginal.cy = bm.bmHeight;
        if (g_hbmWaved != NULL)
            DeleteObject(g_hbmWaved);
        g_hbmWaved = CopyBitmap(g_hbmOriginal);
        g_sizWaved = g_sizOriginal;
        g_nZoomPercent = 100;
        g_xScrollPos = g_yScrollPos = -1;
        g_iImageType = i;
        g_eAmplitude = (double)bm.bmHeight / 4.0;
        g_eWaveLength = (double)bm.bmWidth;
        g_ePhase = 0.0;
        g_fPercent1 = TRUE;
        g_fPercent2 = TRUE;
        g_fVertical = FALSE;
        g_clrBack = CLR_INVALID;
        SetWaveSetting(g_hSetttingDlg);
        ShowWindow(g_hSetttingDlg, SW_SHOWNORMAL);
        SetFileName(pszFileName);
        UpdateWindowCaption();
        OnChange();
        SetCursor(hcurOld);
        return TRUE;
    }
    SetCursor(hcurOld);
    MessageBox(g_hMainWnd, LoadStringDx(17), NULL, MB_ICONERROR);
    return FALSE;
}

UINT APIENTRY CCHookProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CHOOSECOLOR *pcc;
    switch(uMsg)
    {
    case WM_INITDIALOG:
        pcc = (CHOOSECOLOR *)lParam;
        SetWindowText(hDlg, (LPCTSTR)pcc->lCustData);
        return 1;
    }
    return 0;
}

BOOL ChooseColorDx(COLORREF *pclrChoosed, LPCTSTR pszTitle)
{
    CHOOSECOLOR cc;
    static COLORREF aCustColors[16] =
    {
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255)
    };

    ZeroMemory(&cc, sizeof(CHOOSECOLOR));
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner   = g_hMainWnd;
    cc.rgbResult = *pclrChoosed;
    cc.lpCustColors = aCustColors;
    cc.lpfnHook    = CCHookProc;
    cc.Flags       = CC_RGBINIT | CC_ENABLEHOOK;
    cc.lCustData   = (LPARAM)pszTitle;

    *pclrChoosed = CLR_INVALID;
    if (ChooseColor(&cc))
    {
        *pclrChoosed = cc.rgbResult;
        return TRUE;
    }
    return FALSE;
}

BOOL DoSaveFile(LPCTSTR pszFileName)
{
    BOOL f;
    INT i;
    HBITMAP hbm;
    COLORREF clr = g_clrBack;
    HCURSOR hcurWait = LoadCursor(NULL, IDC_WAIT);
    HCURSOR hcurOld = SetCursor(hcurWait);

    i = GetImageType(pszFileName);
    if (i == GIF)
    {
        if (MessageBox(g_hMainWnd, LoadStringDx(19), NULL,
                       MB_ICONWARNING | MB_YESNO) == IDNO)
        {
            SetCursor(hcurOld);
            return FALSE;
        }
    }

    if (g_clrBack != CLR_INVALID || i == JPEG)
    {
        if (g_clrBack == CLR_INVALID && i == JPEG)
            g_clrBack = RGB(255, 255, 255);
        hbm = CreateBackBitmap();
        AlphaBlendBitmap(hbm, g_hbmWaved);
    }
    else
        hbm = g_hbmWaved;
    if (i == JPEG)
        g_clrBack = clr;

    f = FALSE;
    switch(i)
    {
    case BMP:
        f = SaveBitmapToFile(pszFileName, hbm, g_dpi);
        break;

    case GIF:
        if (IsDIBOpaque(hbm))
        {
            f = Save32BppBitmapAsGif(pszFileName, hbm, g_clrTransparent);
        }
        else if (ChooseColorDx(&g_clrTransparent, LoadStringDx(21)))
        {
            f = Save32BppBitmapAsGif(pszFileName, hbm, g_clrTransparent);
        }
        break;

    case JPEG:
        f = SaveBitmapAsJpeg(pszFileName, hbm, 100, FALSE, g_dpi);
        break;

    case PNG:
        f = SaveBitmapAsPngFile(pszFileName, hbm, g_dpi);
        break;

    case TIFF:
        f = SaveBitmapAsTiff(pszFileName, hbm, g_dpi);
        break;
    }
    if (hbm != g_hbmWaved)
        DeleteObject(hbm);
    if (!f)
        MessageBox(g_hMainWnd, LoadStringDx(18), NULL, MB_ICONERROR);

    SetCursor(hcurOld);
    return f;
}

BOOL DoCloseFile(VOID)
{
    SetFileName("");
    UpdateWindowCaption();
    return TRUE;
}

BOOL Open(void)
{
    OPENFILENAME ofn;
    TCHAR szFileName[MAX_PATH];

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    lstrcpyn(szFileName, g_szFileName, MAX_PATH);
    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.hwndOwner       = g_hMainWnd;
    ofn.lpstrFilter     = g_szInputFilter;
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = szFileName;
    ofn.nMaxFile        = MAX_PATH;
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
        OFN_HIDEREADONLY;
    ofn.lpstrDefExt     = "bmp";
    if (GetOpenFileName(&ofn))
    {
        return DoOpenFile(szFileName);
    }
    return FALSE;
}

BOOL SaveAs(void)
{
    OPENFILENAME ofn;
    TCHAR szFileName[MAX_PATH];

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    lstrcpyn(szFileName, g_szFileName, MAX_PATH);
    RemoveExtension(szFileName);
    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.hwndOwner       = g_hMainWnd;
    ofn.lpstrFilter     = g_szOutputFilter;
    switch(g_iImageType)
    {
    case BMP:   ofn.nFilterIndex    = 1; break;
    case GIF:   ofn.nFilterIndex    = 2; break;
    case JPEG:  ofn.nFilterIndex    = 3; break;
    case PNG:   ofn.nFilterIndex    = 4; break;
    case TIFF:  ofn.nFilterIndex    = 5; break;
    }
    ofn.lpstrFile       = szFileName;
    ofn.nMaxFile        = MAX_PATH;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT |
                          OFN_HIDEREADONLY;
    ofn.lpstrDefExt     = TEXT("bmp");
    if (GetSaveFileName(&ofn))
    {
        return DoSaveFile(szFileName);
    }
    return FALSE;
}

BOOL Save(void)
{
    if (g_szFileName[0] == '\0')
        return SaveAs();
    else
        return DoSaveFile(g_szFileName);
}

BOOL CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        MessageBeep(MB_ICONINFORMATION);
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDOK:
            EndDialog(hDlg, IDOK);
            break;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            break;
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rc, rc2;
    TCHAR szFileName[MAX_PATH];
    COLORREF clr;

    switch(uMsg)
    {
    case WM_CREATE:
        g_hCanvasWnd = CreateWindowEx(WS_EX_CLIENTEDGE, g_szCanvasWndClass, "",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)1, g_hInstance, NULL);
        if (g_hCanvasWnd == NULL)
            return -1;
        DragAcceptFiles(hWnd, TRUE);
        if (__argc > 1)
        {
            DoOpenFile(__argv[1]);
        }
        break;

    case WM_DROPFILES:
        DragQueryFile((HDROP)wParam, 0, szFileName, MAX_PATH);
        DoOpenFile(szFileName);
        break;

    case WM_SIZE:
        GetClientRect(hWnd, &rc);
        MoveWindow(g_hCanvasWnd, rc.left, rc.top,
            rc.right - rc.left, rc.bottom - rc.top, TRUE);
        InvalidateRect(g_hCanvasWnd, NULL, TRUE);
        break;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case ID_EXIT:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;

        case ID_OPEN:
            Open();
            break;

        case ID_SAVE:
            Save();
            break;

        case ID_SAVEAS:
            SaveAs();
            break;

        case ID_ZOOMIN:
            g_nZoomPercent *= 2;
            if (g_nZoomPercent > MAX_ZOOM_PERCENT)
                g_nZoomPercent = MAX_ZOOM_PERCENT;
            OnChange();
            break;

        case ID_ZOOMOUT:
            g_nZoomPercent /= 2;
            if (g_nZoomPercent < MIN_ZOOM_PERCENT)
                g_nZoomPercent = MIN_ZOOM_PERCENT;
            OnChange();
            break;

        case ID_ZOOM25:
            g_nZoomPercent = 25;
            OnChange();
            break;

        case ID_ZOOM50:
            g_nZoomPercent = 50;
            OnChange();
            break;

        case ID_ZOOM100:
            g_nZoomPercent = 100;
            OnChange();
            break;

        case ID_ZOOM150:
            g_nZoomPercent = 150;
            OnChange();
            break;

        case ID_ZOOM200:
            g_nZoomPercent = 200;
            OnChange();
            break;

        case ID_BACK:
            if (ChooseColorDx(&g_clrBack, LoadStringDx(20)))
            {
                OnChange();
            }
            break;

        case ID_TRANS:
            if (g_clrBack == CLR_INVALID)
                g_clrBack = RGB(0xFF, 0xFF, 0xFF);
            else
                g_clrBack = CLR_INVALID;
            OnChange();
            break;

        case ID_SETTING:
            if (g_hbmOriginal != NULL)
            {
                if (IsWindowVisible(g_hSetttingDlg))
                    ShowWindow(g_hSetttingDlg, SW_HIDE);
                else
                    ShowWindow(g_hSetttingDlg, SW_SHOWNORMAL);
            }
            break;

        case ID_ABOUT:
            DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT), hWnd,
                AboutDlgProc);
            break;
        }
        break;

    case WM_INITMENUPOPUP:
        switch(g_nZoomPercent)
        {
        case 25: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM25, 0); break;
        case 50: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM50, 0); break;
        case 100: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM100, 0); break;
        case 150: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM150, 0); break;
        case 200: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, ID_ZOOM200, 0); break;
        default: CheckMenuRadioItem((HMENU)wParam, ID_ZOOM25, ID_ZOOM200, -1, 0); break;
        }
        if (g_szFileName[0] != 0)
        {
            EnableMenuItem((HMENU)wParam, ID_SAVE, MF_ENABLED);
            EnableMenuItem((HMENU)wParam, ID_SAVEAS, MF_ENABLED);
        }
        else
        {
            EnableMenuItem((HMENU)wParam, ID_SAVE, MF_GRAYED);
            EnableMenuItem((HMENU)wParam, ID_SAVEAS, MF_GRAYED);
        }
        if (g_clrBack == CLR_INVALID)
            CheckMenuItem((HMENU)wParam, ID_TRANS, MF_CHECKED);
        else
            CheckMenuItem((HMENU)wParam, ID_TRANS, MF_UNCHECKED);
        if (g_nZoomPercent >= MAX_ZOOM_PERCENT)
            EnableMenuItem((HMENU)wParam, ID_ZOOMIN, MF_GRAYED);
        else
            EnableMenuItem((HMENU)wParam, ID_ZOOMIN, MF_ENABLED);
        if (g_nZoomPercent <= MIN_ZOOM_PERCENT)
            EnableMenuItem((HMENU)wParam, ID_ZOOMOUT, MF_GRAYED);
        else
            EnableMenuItem((HMENU)wParam, ID_ZOOMOUT, MF_ENABLED);
        if (IsWindowVisible(g_hSetttingDlg))
            CheckMenuItem((HMENU)wParam, ID_SETTING, MF_CHECKED);
        else
            CheckMenuItem((HMENU)wParam, ID_SETTING, MF_UNCHECKED);
        break;

    case WM_DESTROY:
        DestroyWindow(g_hSetttingDlg);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

INT WINAPI WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       pszCmdLine,
    INT         nCmdShow)
{
    WNDCLASSEX wcx;
    MSG msg;
    BOOL f;
    LPTSTR pch;

    g_hInstance = hInstance;
    InitCommonControls();
    g_dpi = 0.0;

    pch = g_szInputFilter;
    lstrcpy(pch, LoadStringDx(3));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(4));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(5));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(6));
    pch += lstrlen(pch) + 1;
    *pch = 0;

    pch = g_szOutputFilter;
    lstrcpy(pch, LoadStringDx(7));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(8));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(9));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(10));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(11));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(12));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(13));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(14));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(22));
    pch += lstrlen(pch) + 1;
    lstrcpy(pch, LoadStringDx(23));
    pch += lstrlen(pch) + 1;
    *pch = 0;

    wcx.cbSize          = sizeof(WNDCLASSEX);
    wcx.style           = 0;
    wcx.lpfnWndProc     = WindowProc;
    wcx.cbClsExtra      = 0;
    wcx.cbWndExtra      = 0;
    wcx.hInstance       = hInstance;
    wcx.hIcon           = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wcx.hCursor         = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground   = (HBRUSH)(COLOR_3DFACE + 1);
    wcx.lpszMenuName    = MAKEINTRESOURCE(1);
    wcx.lpszClassName   = g_szWndClass;
    wcx.hIconSm         = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON), 0);
    if (!RegisterClassEx(&wcx))
        return 1;

    wcx.lpfnWndProc     = CanvasWndProc;
    wcx.lpszMenuName    = NULL;
    wcx.lpszClassName   = g_szCanvasWndClass;
    if (!RegisterClassEx(&wcx))
        return 1;

    g_hMainWnd = CreateWindow(g_szWndClass, LoadStringDx(1),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0,
        640, 480, NULL, NULL, hInstance, NULL);
    if (g_hMainWnd == NULL)
        return 2;

    g_hSetttingDlg = CreateDialog(hInstance, MAKEINTRESOURCE(1),
        g_hMainWnd, SettingDlgProc);
    if (g_hSetttingDlg == NULL)
    {
        DestroyWindow(g_hMainWnd);
        return 3;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    g_hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(1));

    while((f = GetMessage(&msg, NULL, 0, 0)) != FALSE)
    {
        if (f == -1)
            return -1;
        if (IsDialogMessage(g_hSetttingDlg, &msg))
            continue;
        if (TranslateAccelerator(g_hMainWnd, g_hAccel, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (INT)msg.wParam;
}
