#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <kohn_gif.h>

#ifdef __cplusplus
}
#endif

#include "bitmap.h"

#pragma comment(lib, "kohn_gif.lib")

typedef struct tagBITMAPINFOEX
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} BITMAPINFOEX, FAR * LPBITMAPINFOEX;

HBITMAP LoadGifAsBitmap(LPCSTR pszFileName, COLORREF *prgbTransparent)
{
    struct gif_info_t *gif;
    BITMAPINFOEX bix;
    HBITMAP hbm;
    HDC hDC;
    UINT i, x, y, t, v, widthbytes, num_bytes;
    BYTE *pbBits;

    gif = kgif_alloc_decompress();
    if (gif == NULL)
        return NULL;

    gif->fp = fopen(pszFileName, "rb");
    if (gif->fp == NULL)
    {
        kgif_info_free(gif);
        return NULL;
    }

    if (kgif_read_header(gif) != 0)
    {
        fclose(gif->fp);
        kgif_info_free(gif);
        return NULL;
    }

#define WIDTHBYTES(i) (((i) + 31) / 32 * 4)
    widthbytes = WIDTHBYTES(gif->width * 32);

    ZeroMemory(&bix.bmiHeader, sizeof(BITMAPINFOHEADER));
    bix.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bix.bmiHeader.biWidth       = gif->width;
    bix.bmiHeader.biHeight      = -(INT)gif->height;
    bix.bmiHeader.biPlanes      = 1;
    bix.bmiHeader.biBitCount    = 32;

    hDC = CreateCompatibleDC(NULL);
    if (hDC == NULL)
    {
        fclose(gif->fp);
        kgif_info_free(gif);
        return NULL;
    }

    hbm = CreateDIBSection(hDC, (BITMAPINFO *)&bix, DIB_RGB_COLORS, 
                           (VOID **)&pbBits, NULL, 0);
    DeleteDC(hDC);
    if (hbm == NULL)
    {
        fclose(gif->fp);
        kgif_info_free(gif);
        return NULL;
    }

    if (gif->transparency_index != -1)
    {
        v = gif->color_map[gif->transparency_index];
        *prgbTransparent = RGB(v >> 16, v >> 8, v);
    }
    else
    {
        *prgbTransparent = CLR_INVALID;
    }

    if ((gif->flags & 0x40) == 0x40)
    {
        /* interlace */
        static const int offsets[] = { 0, 4, 2, 1 };
        static const int jumps[] = { 8, 8, 4, 2 };

        x = y = i = 0;
        while (i < 4)
        {
            if (gif->y > (int)y)
                break;

            num_bytes = kgif_decompress(gif);

            for (t = 0; t < num_bytes; t++)
            {
                v = gif->decomp_buff[t];
                if (gif->transparency_index != -1 && (int)v == gif->transparency_index)
                {
                    v = gif->color_map[v];
                    pbBits[y * widthbytes + x * 4 + 0] = (BYTE)v;
                    pbBits[y * widthbytes + x * 4 + 1] = (BYTE)(v >> 8);
                    pbBits[y * widthbytes + x * 4 + 2] = (BYTE)(v >> 16);
                    pbBits[y * widthbytes + x * 4 + 3] = 0;
                }
                else
                {
                    v = gif->color_map[v];
                    pbBits[y * widthbytes + x * 4 + 0] = (BYTE)v;
                    pbBits[y * widthbytes + x * 4 + 1] = (BYTE)(v >> 8);
                    pbBits[y * widthbytes + x * 4 + 2] = (BYTE)(v >> 16);
                    pbBits[y * widthbytes + x * 4 + 3] = 0xFF;
                }
                x++;
                if (x >= gif->width)
                {
                    x = 0;
                    y += jumps[i];
                    if (y >= gif->height)
                    {
                        i++;
                        y = offsets[i];
                    }
                }
            }
        }
    }
    else
    {
        x = y = 0;
        while (y < gif->height)
        {
            if (gif->y > (int)y)
                break;

            num_bytes = kgif_decompress(gif);
            for (t = 0; t < num_bytes; t++)
            {
                v = gif->decomp_buff[t];
                if (gif->transparency_index != -1 && (int)v == gif->transparency_index)
                {
                    v = gif->color_map[v];
                    pbBits[y * widthbytes + x * 4 + 0] = (BYTE)v;
                    pbBits[y * widthbytes + x * 4 + 1] = (BYTE)(v >> 8);
                    pbBits[y * widthbytes + x * 4 + 2] = (BYTE)(v >> 16);
                    pbBits[y * widthbytes + x * 4 + 3] = 0;
                }
                else
                {
                    v = gif->color_map[v];
                    pbBits[y * widthbytes + x * 4 + 0] = (BYTE)v;
                    pbBits[y * widthbytes + x * 4 + 1] = (BYTE)(v >> 8);
                    pbBits[y * widthbytes + x * 4 + 2] = (BYTE)(v >> 16);
                    pbBits[y * widthbytes + x * 4 + 3] = 0xFF;
                }
                x++;
                if (x >= gif->width)
                {
                    x = 0;
                    y++;
                }
            }
        }
    }

    fclose(gif->fp);
    kgif_info_free(gif);
    return hbm;
}

typedef struct tagENTRY
{
    DWORD clr1;
    DWORD clr2;
    DWORD count;
} ENTRY;

static int entry_compare(const void *a, const void *b)
{
    const ENTRY *x = (const ENTRY *)a;
    const ENTRY *y = (const ENTRY *)b;
    if (x->count < y->count) return 1;
    if (x->count > y->count) return -1;
    return 0;
}

static void rgb_to_hsv(BYTE r, BYTE g, BYTE b, double *hue, double *saturation, double *value)
{
    double delta;
    double m0, m1;
    double red, green, blue;
    double h, s, v;

    red   = r / 255.0;
    green = g / 255.0;
    blue  = b / 255.0;
    h = 0.0;

    if (red > green)
    {
        m0 = min(green, blue);
        m1 = max(red,   blue);
    }
    else
    {
        m0 = min(red,   blue);
        m1 = max(green, blue);
    }

    v = m1;

    if (m1 != 0.0)
        s = (m1 - m0) / m1;
    else
        s = 0.0;

    if (s == 0.0)
        h = 0.0;
    else
    {
        delta = m1 - m0;

        if (delta == 0.0)
            delta = 1.0;

        if (red == m1)
            h = (green - blue) / delta;
        else if (green == m1)
            h = 2 + (blue - red) / delta;
        else if (blue == m1)
            h = 4 + (red - green) / delta;

        h /= 6.0;
        if (h < 0.0)
            h += 1.0;
        else if (h > 1.0)
            h -= 1.0;
    }

    *hue        = h;
    *saturation = s;
    *value      = v;
}

BOOL Save32BppBitmapAsGif(LPCSTR pszFileName, HBITMAP hbm, COLORREF clrTransparent)
{
    struct gif_info_t *gif;
    BITMAP bm;
    HBITMAP hbmBlended;
    DWORD cdw, *pdw, *pdw2;
    DWORD clr;
#define TABLE_SIZE 1024
    ENTRY table[TABLE_SIZE], *p;
    INT count, i, x, y;
    VOID *pvBits;
    BYTE *pb;
    INT iNearest, widthbytes;
    double norm, nearest_norm;
    double h1, s1, v1, h2, s2, v2, dh;
    BOOL fAlpha;

    if (!GetObject(hbm, sizeof(BITMAP), &bm))
        return FALSE;

    fAlpha = (bm.bmBitsPixel == 32);

    gif = kgif_alloc_compress();
    if (gif == NULL)
        return FALSE;

    gif->fp = fopen(pszFileName, "wb");
    if (gif->fp == NULL)
    {
        kgif_info_free(gif);
        return FALSE;
    }

    gif->width = bm.bmWidth;
    gif->height = bm.bmHeight;

    if (clrTransparent == CLR_INVALID)
        clrTransparent = RGB(255, 255, 255);

    hbmBlended = CreateSolid32BppBitmap(bm.bmWidth, bm.bmHeight, clrTransparent);
    if (hbmBlended == NULL)
    {
        fclose(gif->fp);
        kgif_info_free(gif);
        DeleteFile(pszFileName);
        return FALSE;
    }

    AlphaBlendBitmap(hbmBlended, hbm);

    count = 0;
    GetObject(hbmBlended, sizeof(BITMAP), &bm);
    pdw = (DWORD *)bm.bmBits;
    cdw = bm.bmWidth * bm.bmHeight;
    while(cdw--)
    {
        clr = *pdw++ & 0xFFFFFF;  /* BGR */
        table[count].clr1 = clr;
        clr &= 0xF0F0F0;
        table[count].clr2 = clr;
        table[count].count = 0;
        p = table;
        while(clr != p->clr2)
            p++;
        p->count += 1;
        if (p == &table[count] && count < TABLE_SIZE - 1)
            count++;
    }
    qsort(table, count, sizeof(ENTRY), entry_compare);
    if (count > 255)
        count = 255;
    gif->colors = count + 1;
    gif->transparency_index = (fAlpha ? count : -1);

    for(i = 0; i < count; i++)
        gif->color_map[i] = table[i].clr1;
    gif->color_map[count] = 
        (GetRValue(clrTransparent) << 16) |
        (GetGValue(clrTransparent) << 8) |
        GetBValue(clrTransparent);

    pdw = (DWORD *)bm.bmBits;
    if (fAlpha && GetObject(hbm, sizeof(BITMAP), &bm))
        pdw2 = (DWORD *)bm.bmBits;
#define WIDTHBYTES(i) (((i) + 31) / 32 * 4)
    widthbytes = WIDTHBYTES(bm.bmWidth * 8);
    if (kgif_write_header(gif) != 0 || 
        (pvBits = malloc(widthbytes * bm.bmHeight)) == NULL)
    {
        fclose(gif->fp);
        kgif_info_free(gif);
        DeleteFile(pszFileName);
        DeleteObject(hbmBlended);
        return FALSE;
    }

    for(y = 0; y < bm.bmHeight; y++)
    {
        pb = (BYTE *)pvBits + widthbytes * y;
        for(x = 0; x < bm.bmWidth; x++)
        {
            if (fAlpha && (BYTE)(*pdw2 >> 24) == 0)
            {
                *pb = (BYTE)count;
            }
            else
            {
                clr = *pdw & 0xF0F0F0;
                for(i = 0; i < count; i++)
                {
                    if (table[i].clr2 == clr)
                    {
                        *pb = (BYTE)i;
                        break;
                    }
                }
                if (i == count)
                {
                    rgb_to_hsv((BYTE)(clr >> 16), (BYTE)(clr >> 8), (BYTE)clr, 
                        &h1, &s1, &v1);
                    iNearest = 0;
                    nearest_norm = 100000.0;
                    for(i = 0; i < count; i++)
                    {
                        rgb_to_hsv(
                            (BYTE)(table[i].clr1 >> 16),
                            (BYTE)(table[i].clr1 >> 8),
                            (BYTE)table[i].clr1, &h2, &s2, &v2);
                        dh = fabs(h2 - h1);
                        if (dh > 0.5)
                            dh = 1.0 - dh;
                        norm = dh * dh + (s2 - s1) * (s2 - s1) + (v2 - v1) * (v2 - v1);
                        if (norm < nearest_norm)
                        {
                            iNearest = i;
                            nearest_norm = norm;
                            if (norm == 0.0)
                                break;
                        }
                    }
                    *pb = (BYTE)iNearest;
                }
            }
            pdw++;
            pdw2++;
            pb++;
        }
    }
    for(y = bm.bmHeight - 1; y >= 0; y--)
    {
        pb = (BYTE *)pvBits + widthbytes * y;
        kgif_compress_block(gif, pb, gif->width);
    }
    free(pvBits);
    DeleteObject(hbmBlended);
    kgif_write_footer(gif);
    fclose(gif->fp);
    kgif_info_free(gif);
    return TRUE;
}
