#include <windows.h>
#include <math.h>

typedef struct tagBITMAPINFOEX
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} BITMAPINFOEX, FAR * LPBITMAPINFOEX;

BOOL SaveBitmapToFile(LPCTSTR pszFileName, HBITMAP hbm)
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
    LPVOID pBits;
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

int main(void)
{
    HDC hdc;
    HBITMAP hbm;
    HGDIOBJ hbmOld;
    BITMAPINFO bi;
    VOID *pvBits;

    hdc = CreateCompatibleDC(NULL);
    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER); 
    bi.bmiHeader.biWidth    = 128;
    bi.bmiHeader.biHeight   = 128; 
    bi.bmiHeader.biPlanes   = 1; 
    bi.bmiHeader.biBitCount = 32;
    hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    hbmOld = SelectObject(hdc, hbm);
    {
        int i;
        int x, y;
        HPEN hPen;
        HGDIOBJ hPenOld;
        HBRUSH hbr;
        RECT rc = {0, 0, 128, 128};
        hbr = CreateSolidBrush(RGB(255, 255, 0));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        
        hPen = CreatePen(PS_SOLID, 10, RGB(0, 0, 0));
        hPenOld = SelectObject(hdc, hPen);
        
        x = 6;
        y = 64;
        MoveToEx(hdc, x, y, NULL);
        for(i = 6; i < 128 - 6; i++)
        {
            x = i;
            y = 64 - (int)floor(20.0 * sin(2.0 * M_PI * (x - 6) / (128 - 12)) + 0.5);
            LineTo(hdc, x, y);
        }
        SelectObject(hdc, hPenOld);
        DeleteObject(hPen);
    }
    SelectObject(hdc, hbmOld);
    SaveBitmapToFile("wavepix-128x128.bmp", hbm);
    DeleteObject(hbm);
    return 0;
}
