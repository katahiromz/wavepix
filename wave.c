#include <windows.h>

#include <tchar.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "wavepix.h"


double g_eAmplitude, g_eWaveLength, g_ePhase;
BOOL g_fVertical;
HBITMAP g_hbmOriginal;
SIZE g_sizOriginal;
HBITMAP g_hbmWaved;
SIZE g_sizWaved;
COLORREF g_clrBack;
LPTSTR g_pszInput;
LPTSTR g_pszOutput;
float g_dpi;
COLORREF g_clrTransparent;

BOOL g_fPercent1;
BOOL g_fPercent2;

void Init(void)
{
    g_eAmplitude = 25.0;
    g_eWaveLength = 100.0;
    g_ePhase = 0;
    g_fVertical = FALSE;
    g_hbmOriginal = NULL;
    g_hbmWaved = NULL;
    g_clrBack = CLR_INVALID;
    g_pszInput = NULL;
    g_pszOutput = NULL;
    g_dpi = 0;
    g_clrTransparent = CLR_INVALID;
    g_fPercent1 = TRUE;
    g_fPercent2 = TRUE;
}

void Help(void)
{
    printf("wave - 画像を波状に変形します。\n");
    printf("使い方: wave -i 入力 -o 出力 [-a 振幅] [-l 波長] [-p 位相] [-v ON/OFF]\n");
    printf("             [-b #RRGGBB] [-t #RRGGBB]\n");
    printf("\n");
    printf("-aと-lには、ピクセル単位か%で指定できます。\n");
    printf("-pは、角度で指定します。\n");
    printf("-v ONは縦向きに変形するときに指定します。\n");
    printf("-b には背景色を指定します。\n");
    printf("-t にはGIFの透過色を指定します。\n");
}

void Version(void)
{
    printf("wave ver. 0.3 by 片山博文MZ\n");
    printf("katayama.hirofumi.mz@gmail.com\n");
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

BOOL Open(LPCTSTR pszFileName)
{
    HBITMAP hbm;
    float dpi;
    INT i;
    
    i = GetImageType(pszFileName);
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
        g_dpi = dpi;
        g_hbmOriginal = hbm;
        return TRUE;
    }
    return FALSE;
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

BOOL Save(LPCTSTR pszFileName)
{
    HBITMAP hbm;
    INT i;
    BOOL f;
    COLORREF clr = g_clrBack;

    i = GetImageType(pszFileName);

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
        else
        {
            if (g_clrTransparent == CLR_INVALID)
                g_clrTransparent = g_clrBack;
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

    return f;
}

int main(int argc, char **argv)
{
    int i;
    BOOL f;
    
    if (argc == 1)
    {
        Help();
        return 0;
    }
    if (argc == 2 && lstrcmpi(argv[1], "--help"))
    {
        Help();
        return 0;
    }
    if (argc == 2 && lstrcmpi(argv[1], "--version"))
    {
        Version();
        return 0;
    }

    Init();
    f = TRUE;
    for(i = 1; i < argc; i++)
    {
        if (lstrcmpi(argv[i], "-a") == 0)
        {
            if (i + 1 < argc)
            {
                LPTSTR pch;
                g_eAmplitude = atof(argv[++i]);
                pch = argv[i];
                while(isdigit(*pch) || *pch == '.')
                {
                    pch++;
                }
                g_fPercent1 = (*pch == '%');
            }
            else
            {
                f = FALSE;
            }
        }
        if (lstrcmpi(argv[i], "-l") == 0)
        {
            if (i + 1 < argc)
            {
                LPTSTR pch;
                g_eWaveLength = atof(argv[++i]);
                pch = argv[i];
                while(isdigit(*pch) || *pch == '.')
                {
                    pch++;
                }
                g_fPercent2 = (*pch == '%');
            }
            else
            {
                f = FALSE;
            }
        }
        if (lstrcmpi(argv[i], "-p") == 0)
        {
            if (i + 1 < argc)
            {
                g_ePhase = atof(argv[++i]);
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-i") == 0)
        {
            if (i + 1 < argc)
            {
                g_pszInput = argv[++i];
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                g_pszOutput = argv[++i];
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-v") == 0)
        {
            if (i + 1 < argc)
            {
                if (lstrcmpi(argv[++i], "ON") == 0)
                {
                    g_fVertical = TRUE;
                }
                else if (lstrcmpi(argv[++i], "OFF") == 0)
                {
                    g_fVertical = FALSE;
                }
                else
                {
                    f = FALSE;
                }
            }
        }
        else if (lstrcmpi(argv[i], "-b") == 0)
        {
            if (i + 1 < argc && argv[++i][0] == '#')
            {
                DWORD dw = strtoul(argv[i] + 1, NULL, 16);
                g_clrBack = RGB((BYTE)HIWORD(dw), HIBYTE(LOWORD(dw)), (BYTE)dw);
            }
            else
            {
                f = FALSE;
            }
        }
        else if (lstrcmpi(argv[i], "-t") == 0)
        {
            if (i + 1 < argc && argv[++i][0] == '#')
            {
                DWORD dw = strtoul(argv[i] + 1, NULL, 16);
                g_clrTransparent = RGB((BYTE)HIWORD(dw), HIBYTE(LOWORD(dw)), (BYTE)dw);
            }
            else
            {
                f = FALSE;
            }
        }
    }
    if (g_pszInput == NULL || g_pszOutput == NULL || !f)
    {
        printf("引数が間違っています。\n");
        Help();
        return 1;
    }

    if (!Open(g_pszInput))
    {
        printf("%s: ファイルが開けないか、形式が間違っています。\n", g_pszInput);
        return 2;
    }

    {
        BITMAP bm;
        GetObject(g_hbmOriginal, sizeof(BITMAP), &bm);
        g_sizOriginal.cx = bm.bmWidth;
        g_sizOriginal.cy = bm.bmHeight;
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
    }

    if (g_eWaveLength == 0.0)
    {
        printf("波長の値が不正です。\n");
        DeleteObject(g_hbmOriginal);
        return 6;
    }

    g_hbmWaved = CreateWaved32BppBitmap(g_hbmOriginal, g_eAmplitude, g_eWaveLength, g_ePhase, g_fVertical);
    if (g_hbmWaved == NULL)
    {
        printf("メモリが足りません。\n");
        DeleteObject(g_hbmOriginal);
        return 2;
    }
    else
    {
        BITMAP bm;
        GetObject(g_hbmWaved, sizeof(BITMAP), &bm);
        g_sizWaved.cx = bm.bmWidth;
        g_sizWaved.cy = bm.bmHeight;
    }

    if (!Save(g_pszOutput))
    {
        printf("%s: ファイルが出力できませんでした。\n", g_pszOutput);
        DeleteObject(g_hbmOriginal);
        DeleteObject(g_hbmWaved);
        return 3;
    }

    DeleteObject(g_hbmOriginal);
    DeleteObject(g_hbmWaved);

    return 0;
}
