#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct tagBITMAPINFOEX
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} BITMAPINFOEX, FAR * LPBITMAPINFOEX;

HBITMAP CreateWaved32BppBitmap(
    HBITMAP hbmSrc, 
    double amplitude, 
    double length, 
    double phase, 
    BOOL fVertical)
{
    HDC hdc;
    HBITMAP hbm;
    BITMAP bm;
    BITMAPINFO bi;
    BYTE *pbBits, *pbBitsSrc;
    LONG widthbytes, widthbytesSrc;
    INT cx, cy, x, y, x0, y0;
    double dx, dy;
    DWORD dw;
    BOOL fAlpha;

    if (!GetObject(hbmSrc, sizeof(BITMAP), &bm))
        return NULL;

    if (fVertical)
    {
        cx = bm.bmWidth + fabs(amplitude) * 2.0 + 0.5;
        cy = bm.bmHeight;
    }
    else
    {
        cx = bm.bmWidth;
        cy = bm.bmHeight + fabs(amplitude) * 2.0 + 0.5;
    }

    hdc = CreateCompatibleDC(NULL);
    if (hdc == NULL)
        return NULL;

    fAlpha = (bm.bmBitsPixel == 32);
    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = bm.bmWidth;
    bi.bmiHeader.biHeight   = bm.bmHeight;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    widthbytesSrc = bm.bmWidth * 4;
    pbBitsSrc = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 
                                  widthbytesSrc * bm.bmHeight);
    if (pbBitsSrc == NULL)
    {
        DeleteDC(hdc);
        return NULL;
    }
    GetDIBits(hdc, hbmSrc, 0, bm.bmHeight, pbBitsSrc, &bi, DIB_RGB_COLORS);

    bi.bmiHeader.biWidth    = cx;
    bi.bmiHeader.biHeight   = cy;
    widthbytes = cx * 4;
    hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (VOID **)&pbBits,
                           NULL, 0);
    if (hbm == NULL)
    {
        HeapFree(GetProcessHeap(), 0, pbBitsSrc);
        DeleteDC(hdc);
        return NULL;
    }
    ZeroMemory(pbBits, widthbytes * cy);

    if (fVertical)
    {
        for(y = 0; y < cy; y++)
        {
            dx = amplitude * sin(2.0 * M_PI * y / length + phase);
            for(x = 0; x < cx; x++)
            {
                x0 = x + (INT)floor(dx - amplitude + 0.5);
                if (0 <= x0 && x0 < bm.bmWidth)
                {
                    dw = *(DWORD *)&pbBitsSrc[x0 * 4 + y * widthbytesSrc];
                    if (!fAlpha)
                        dw |= 0xFF000000;
                    *(DWORD *)&pbBits[x * 4 + y * widthbytes] = dw;
                }
            }
        }
    }
    else
    {
        for(x = 0; x < cx; x++)
        {
            dy = -amplitude * sin(2.0 * M_PI * x / length + phase);
            for(y = 0; y < cy; y++)
            {
                y0 = y + (INT)floor(dy - amplitude + 0.5);
                if (0 <= y0 && y0 < bm.bmHeight)
                {
                    dw = *(DWORD *)&pbBitsSrc[x * 4 + y0 * widthbytesSrc];
                    if (!fAlpha)
                        dw |= 0xFF000000;
                    *(DWORD *)&pbBits[x * 4 + y * widthbytes] = dw;
                }
            }
        }
    }
    HeapFree(GetProcessHeap(), 0, pbBitsSrc);
    DeleteDC(hdc);
    return hbm;
}

HBITMAP LoadBitmapFromFile(LPCTSTR pszFileName, float *dpi)
{
    HANDLE hFile;
    BITMAPFILEHEADER bf;
    BITMAPINFOEX bi;
    DWORD cb, cbImage;
    DWORD dwError;
    LPVOID pBits, pBits2;
    HDC hDC, hMemDC;
    HBITMAP hbm;

    hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;

    if (!ReadFile(hFile, &bf, sizeof(BITMAPFILEHEADER), &cb, NULL))
    {
        dwError = GetLastError();
        CloseHandle(NULL);
        SetLastError(dwError);
        return NULL;
    }

    pBits = NULL;
    if (bf.bfType == 0x4D42 && bf.bfReserved1 == 0 && bf.bfReserved2 == 0 &&
        bf.bfSize > bf.bfOffBits && bf.bfOffBits > sizeof(BITMAPFILEHEADER) &&
        bf.bfOffBits <= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOEX))
    {
        if (ReadFile(hFile, &bi, bf.bfOffBits -
                     sizeof(BITMAPFILEHEADER), &cb, NULL))
        {
#ifndef LR_LOADREALSIZE
#define LR_LOADREALSIZE 128
#endif
            *dpi = bi.bmiHeader.biXPelsPerMeter * 2.54 / 100.0;
            hbm = (HBITMAP)LoadImage(NULL, pszFileName, IMAGE_BITMAP, 
                0, 0, LR_LOADFROMFILE | LR_LOADREALSIZE | 
                LR_CREATEDIBSECTION);
            if (hbm != NULL)
            {
                CloseHandle(hFile);
                return hbm;
            }
            cbImage = bf.bfSize - bf.bfOffBits;
            pBits = HeapAlloc(GetProcessHeap(), 0, cbImage);
            if (pBits != NULL)
            {
                if (ReadFile(hFile, pBits, cbImage, &cb, NULL))
                {
                    ;
                }
                else
                {
                    dwError = GetLastError();
                    HeapFree(GetProcessHeap(), 0, pBits);
                    pBits = NULL;
                }
            }
            else
                dwError = GetLastError();
        }
        else
            dwError = GetLastError();
    }
    else
        dwError = ERROR_INVALID_DATA;
    CloseHandle(hFile);

    if (pBits == NULL)
    {
        SetLastError(dwError);
        return NULL;
    }

    hbm = NULL;
    hDC = GetDC(NULL);
    if (hDC != NULL)
    {
        hMemDC = CreateCompatibleDC(hDC);
        if (hMemDC != NULL)
        {
            hbm = CreateDIBSection(hMemDC, (BITMAPINFO*)&bi, DIB_RGB_COLORS,
                                   &pBits2, NULL, 0);
            if (hbm != NULL)
            {
                if (SetDIBits(hMemDC, hbm, 0, abs(bi.bmiHeader.biHeight),
                              pBits, (BITMAPINFO*)&bi, DIB_RGB_COLORS))
                {
                    ;
                }
                else
                {
                    dwError = GetLastError();
                    DeleteObject(hbm);
                    hbm = NULL;
                }
            }
            else
                dwError = GetLastError();

            DeleteDC(hMemDC);
        }
        else
            dwError = GetLastError();

        ReleaseDC(NULL, hDC);
    }
    else
        dwError = GetLastError();

    HeapFree(GetProcessHeap(), 0, pBits);
    SetLastError(dwError);

    return hbm;
}

BOOL SaveBitmapToFile(LPCTSTR pszFileName, HBITMAP hbm, float dpi)
{
    BOOL f;
    DWORD dwError;
    BITMAPFILEHEADER bf;
    BITMAPINFOEX bi;
    BITMAPINFOHEADER *pbmih;
    DWORD cb;
    DWORD cColors, cbColors;
    HDC hDC;
    HANDLE hFile;
    VOID *pBits;
    BITMAP bm;

    if (!GetObject(hbm, sizeof(BITMAP), &bm))
        return FALSE;

    pbmih = &bi.bmiHeader;
    ZeroMemory(pbmih, sizeof(BITMAPINFOHEADER));
    pbmih->biSize             = sizeof(BITMAPINFOHEADER);
    pbmih->biWidth            = bm.bmWidth;
    pbmih->biHeight           = bm.bmHeight;
    pbmih->biPlanes           = 1;
    pbmih->biBitCount         = bm.bmBitsPixel;
    pbmih->biCompression      = BI_RGB;
    pbmih->biSizeImage        = bm.bmWidthBytes * bm.bmHeight;
    if (dpi != 0.0)
    {
        pbmih->biXPelsPerMeter = pbmih->biYPelsPerMeter = dpi * 100 / 2.54 + 0.5;
    }

    if (bm.bmBitsPixel < 16)
        cColors = 1 << bm.bmBitsPixel;
    else
        cColors = 0;
    cbColors = cColors * sizeof(RGBQUAD);

    bf.bfType = 0x4d42;
    bf.bfReserved1 = 0;
    bf.bfReserved2 = 0;
    cb = sizeof(BITMAPFILEHEADER) + pbmih->biSize + cbColors;
    bf.bfOffBits = cb;
    bf.bfSize = cb + pbmih->biSizeImage;

    pBits = HeapAlloc(GetProcessHeap(), 0, pbmih->biSizeImage);
    if (pBits == NULL)
        return FALSE;

    f = FALSE;
    hDC = GetDC(NULL);
    if (hDC != NULL)
    {
        if (GetDIBits(hDC, hbm, 0, bm.bmHeight, pBits, (BITMAPINFO*)&bi, 
            DIB_RGB_COLORS))
        {
            hFile = CreateFile(pszFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | 
                               FILE_FLAG_WRITE_THROUGH, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                f = WriteFile(hFile, &bf, sizeof(BITMAPFILEHEADER), &cb, NULL) &&
                    WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &cb, NULL) &&
                    WriteFile(hFile, bi.bmiColors, cbColors, &cb, NULL) &&
                    WriteFile(hFile, pBits, pbmih->biSizeImage, &cb, NULL);
                if (!f)
                    dwError = GetLastError();
                CloseHandle(hFile);

                if (!f)
                    DeleteFile(pszFileName);
            }
            else
                dwError = GetLastError();
        }
        else
            dwError = GetLastError();
        ReleaseDC(NULL, hDC);
    }
    else
        dwError = GetLastError();

    HeapFree(GetProcessHeap(), 0, pBits);
    SetLastError(dwError);
    return f;
}

HBITMAP CopyBitmap(HBITMAP hbm)
{
    return (HBITMAP)CopyImage(hbm, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
}

HBITMAP CreateSolid32BppBitmap(INT cx, INT cy, COLORREF clr)
{
    BITMAPINFO bi;
    HDC hdc;
    HBITMAP hbm;
    HGDIOBJ hbmOld;
    VOID *pvBits;
    RECT rc;
    HBRUSH hbr;
    DWORD cdw;
    BYTE *pb;

    hdc = CreateCompatibleDC(NULL);
    if (hdc == NULL)
        return NULL;

    hbr = CreateSolidBrush(clr);
    if (hbr == NULL)
    {
        DeleteDC(hdc);
        return NULL;
    }

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = cx;
    bi.bmiHeader.biHeight   = cy;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (hbm != NULL)
    {
        rc.left = 0;
        rc.top = 0;
        rc.right = cx;
        rc.bottom = cy;
        hbmOld = SelectObject(hdc, hbm);
        FillRect(hdc, &rc, hbr);
        SelectObject(hdc, hbmOld);
        cdw = cx * cy;
        pb = (BYTE *)pvBits;
        while(cdw--)
        {
            pb++;
            pb++;
            pb++;
            *pb++ = 0xFF;
        }
    }
    DeleteObject(hbr);
    DeleteDC(hdc);
    return hbm;
}

BOOL IsDIBOpaque(HBITMAP hbm)
{
    DWORD cdw;
    BYTE *pb;
    BITMAP bm;
    GetObject(hbm, sizeof(BITMAP), &bm);
    if (bm.bmBitsPixel <= 24)
        return TRUE;
    cdw = bm.bmWidth * bm.bmHeight;
    pb = (BYTE *)bm.bmBits;
    while(cdw--)
    {
        pb++;
        pb++;
        pb++;
        if (*pb++ != 0xFF)
            return FALSE;
    }
    return TRUE;
}

BOOL AlphaBlendBitmap(HBITMAP hbm1, HBITMAP hbm2)
{
    BITMAP bm1, bm2;
    BYTE *pb1, *pb2;
    DWORD cdw;
    INT x, y;
    BYTE a1, a2;
    GetObject(hbm1, sizeof(BITMAP), &bm1);
    GetObject(hbm2, sizeof(BITMAP), &bm2);

    if (bm1.bmBitsPixel == 32 && bm2.bmBitsPixel == 32)
    {
        pb1 = (BYTE *)bm1.bmBits;
        pb2 = (BYTE *)bm2.bmBits;
        cdw = bm1.bmWidth * bm1.bmHeight;
        while(cdw--)
        {
            a2 = pb2[3];
            a1 = (BYTE)(255 - a2);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
            *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
        }
        return TRUE;
    }

    if (bm1.bmBitsPixel == 24 && bm2.bmBitsPixel == 32)
    {
        pb1 = (BYTE *)bm1.bmBits;
        pb2 = (BYTE *)bm2.bmBits;
        for(y = 0; y < bm1.bmHeight; y++)
        {
            for(x = 0; x < bm1.bmWidth; x++)
            {
                a2 = pb2[3];
                a1 = (BYTE)(255 - a2);
                *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
                *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
                *pb1++ = (BYTE)((a1 * *pb1 + a2 * *pb2++) / 255);
                pb2++;
            }
            pb1 += bm1.bmWidthBytes - bm1.bmWidth * 3;
        }
        return TRUE;
    }
    return FALSE;
}
