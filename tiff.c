#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>

#pragma comment(lib, "libtiff.lib")

HBITMAP LoadTiffAsBitmap(LPCSTR pszFileName, float *dpi)
{
    TIFF* tif;
    BITMAPINFO bi;
    HBITMAP hbm, hbm32Bpp, hbm24Bpp;
    DWORD *pdwBits, *pdw;
    VOID *pvBits;
    HDC hdc1, hdc2;
    uint16 resunit;

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biPlanes   = 1; 
    bi.bmiHeader.biBitCount = 32;

    hdc1 = CreateCompatibleDC(NULL);
    if (hdc1 == NULL)
        return FALSE;

    hdc2 = CreateCompatibleDC(NULL);
    if (hdc2 == NULL)
    {
        DeleteDC(hdc1);
        return FALSE;
    }

    hbm = NULL;
    tif = TIFFOpen(pszFileName, "r");
    if (tif != NULL)
    {
        uint32 w, h;
        size_t cPixels;
        uint8 r, g, b, a;
        BOOL fOpaque;
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
        TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &resunit);
        if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, dpi))
        {
            *dpi = 0.0;
        }
        else if (resunit == RESUNIT_CENTIMETER)
        {
            *dpi *= 2.54;
        }
        bi.bmiHeader.biWidth    = w;
        bi.bmiHeader.biHeight   = h;
        cPixels = w * h;
        hbm32Bpp = CreateDIBSection(hdc1, &bi, DIB_RGB_COLORS, 
            (VOID **)&pdwBits, NULL, 0);
        if (hbm32Bpp != NULL)
        {
            if (TIFFReadRGBAImageOriented(tif, w, h, (uint32 *)pdwBits, 
                ORIENTATION_BOTLEFT, 0))
            {
                pdw = pdwBits;
                fOpaque = TRUE;
                while(cPixels--)
                {
                    r = (uint8)TIFFGetR(*pdw);
                    g = (uint8)TIFFGetG(*pdw);
                    b = (uint8)TIFFGetB(*pdw);
                    a = (uint8)TIFFGetA(*pdw);
                    if (a != 255)
                    {
                        fOpaque = FALSE;
                    }
                    *pdw++ = (uint32)b | (((uint32)g) << 8) | 
                        (((uint32)r) << 16) | (((uint32)a) << 24);
                }
                if (fOpaque)
                {
                    bi.bmiHeader.biBitCount = 24;
                    hbm24Bpp = CreateDIBSection(hdc2, &bi, DIB_RGB_COLORS,
                        &pvBits, NULL, 0);
                    if (hbm24Bpp != NULL)
                    {
                        HGDIOBJ hbmOld1 = SelectObject(hdc1, hbm32Bpp);
                        HGDIOBJ hbmOld2 = SelectObject(hdc2, hbm24Bpp);
                        BitBlt(hdc2, 0, 0, w, h, hdc1, 0, 0, SRCCOPY);
                        SelectObject(hdc1, hbmOld1);
                        SelectObject(hdc2, hbmOld2);
                        hbm = hbm24Bpp;
                        DeleteObject(hbm32Bpp);
                    }
                }
                else
                {
                    hbm = hbm32Bpp;
                }
            }
        }
        TIFFClose(tif);
    }
    DeleteDC(hdc1);
    DeleteDC(hdc2);

    return hbm;
}

BOOL IsDIBOpaque(HBITMAP hbm);

BOOL SaveBitmapAsTiff(LPCSTR pszFileName, HBITMAP hbm, float dpi)
{
    BITMAP bm;
    BOOL no_alpha;
    TIFF *tif;
    BITMAPINFO bi;
    LONG widthbytes;
    BYTE *pbBits, *pb;
    INT c, y;
    BOOL f;
    HDC hdc;

    if (!GetObject(hbm, sizeof(BITMAP), &bm))
        return FALSE;

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = bm.bmWidth;
    bi.bmiHeader.biHeight   = bm.bmHeight;
    bi.bmiHeader.biPlanes   = 1;

    no_alpha = (bm.bmBitsPixel <= 24 || IsDIBOpaque(hbm));
    bi.bmiHeader.biBitCount = (WORD)(no_alpha ? 24 : 32);
#define WIDTHBYTES(i) (((i) + 31) / 32 * 4)
    widthbytes = WIDTHBYTES(bm.bmWidth * bi.bmiHeader.biBitCount);
    pbBits = (BYTE *)malloc(widthbytes * bm.bmHeight);
    if (pbBits == NULL)
        return FALSE;

    hdc = CreateCompatibleDC(NULL);
    if (!GetDIBits(hdc, hbm, 0, bm.bmHeight, pbBits, &bi, DIB_RGB_COLORS))
    {
        free(pbBits);
        DeleteDC(hdc);
        return FALSE;
    }
    DeleteDC(hdc);

    tif = TIFFOpen(pszFileName, "w");
    if (tif == NULL)
    {
        free(pbBits);
        return FALSE;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, bm.bmWidth);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, bm.bmHeight);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, no_alpha ? 3 : 4);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    if (dpi != 0.0)
    {
        TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
        TIFFSetField(tif, TIFFTAG_XRESOLUTION, dpi);
        TIFFSetField(tif, TIFFTAG_YRESOLUTION, dpi);
    }
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "katayama_hirofumi_mz's software");
    f = TRUE;
    for(y = 0; y < bm.bmHeight; y++)
    {
        pb = &pbBits[(bm.bmHeight - 1 - y) * widthbytes];
        c = bm.bmWidth;
        if (no_alpha)
        {
            BYTE b;
            while(c--)
            {
                b = pb[2];
                pb[2] = *pb;
                *pb = b;
                pb++; pb++; pb++;
            }
        }
        else
        {
            BYTE b;
            while(c--)
            {
                b = pb[2];
                pb[2] = *pb;
                *pb = b;
                pb++; pb++; pb++; pb++;
            }
        }
        if (TIFFWriteScanline(tif, 
            &pbBits[(bm.bmHeight - 1 - y) * widthbytes], y, 0) < 0)
        {
            f = FALSE;
            break;
        }
    }
    TIFFClose(tif);

    if (!f)
        DeleteFile(pszFileName);

    free(pbBits);
    return f;
}
