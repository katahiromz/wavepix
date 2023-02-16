#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <png.h>

#include <setjmp.h>

#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "libpng.lib")

#define WIDTHBYTES(i) (((i) + 31) / 32 * 4)

HBITMAP LoadPngAsBitmap(LPCSTR pszFileName, float *dpi)
{
    FILE            *inf;
    HBITMAP         hbm;
    png_structp     png;
    png_infop       info;
    png_uint_32     y, width, height, rowbytes;
    int             color_type, depth, widthbytes;
    double          gamma;
    BITMAPINFO      bi;
    BYTE            *pbBits;
    png_uint_32     res_x, res_y;
    int             unit_type;

    inf = fopen(pszFileName, "rb");
    if (inf == NULL)
        return NULL;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL)
    {
        fclose(inf);
        return NULL;
    }

    info = png_create_info_struct(png);
    if (info == NULL || setjmp(png->jmpbuf))
    {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(inf);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, png_infopp_NULL);
        fclose(inf);
        return NULL;
    }

    png_init_io(png, inf);
    png_read_info(png, info);

    png_get_IHDR(png, info, &width, &height, &depth, &color_type,
                 NULL, NULL, NULL);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_palette_to_rgb(png);
    png_set_bgr(png);
    if (png_get_gAMA(png, info, &gamma))
        png_set_gamma(png, 2.2, gamma);
    else
        png_set_gamma(png, 2.2, 0.45455);

    png_read_update_info(png, info);
    png_get_IHDR(png, info, &width, &height, &depth, &color_type,
                 NULL, NULL, NULL);

    *dpi = 0.0;
    if (png_get_pHYs(png, info, &res_x, &res_y, &unit_type))
    {
        if (unit_type == PNG_RESOLUTION_METER)
            *dpi = res_x * 2.54 / 100.0;
    }

#ifdef PNG_FREE_ME_SUPPORTED
    png_free_data(png, info, PNG_FREE_ROWS, 0);
#endif

    rowbytes = png_get_rowbytes(png, info);
    if (info->row_pointers == NULL)
    {
        info->row_pointers = (png_bytepp)png_malloc(png,
            info->height * png_sizeof(png_bytep));
#ifdef PNG_FREE_ME_SUPPORTED
        info->free_me |= PNG_FREE_ROWS;
#endif
        for (y = 0; y < info->height; y++)
        {
            info->row_pointers[y] = (png_bytep)png_malloc(png, rowbytes);
        }
    }
    png_read_image(png, info->row_pointers);
    info->valid |= PNG_INFO_IDAT;
    png_read_end(png, NULL);
    fclose(inf);

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = width;
    bi.bmiHeader.biHeight      = height;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = (WORD)(depth * png_get_channels(png, info));

    hbm = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (VOID **)&pbBits, 
                           NULL, 0);
    if (hbm == NULL)
    {
        png_destroy_read_struct(&png, &info, png_infopp_NULL);
        return NULL;
    }

    widthbytes = WIDTHBYTES(width * bi.bmiHeader.biBitCount);
    for(y = 0; y < height; y++)
    {
        CopyMemory(pbBits + y * widthbytes, 
                   info->row_pointers[height - 1 - y], rowbytes);
    }

    png_destroy_read_struct(&png, &info, png_infopp_NULL);
    return hbm;
}

BOOL SaveBitmapAsPngFile(LPCSTR pszFileName, HBITMAP hbm, float dpi)
{
    png_structp png;
    png_infop info;
    png_color_8 sBIT;
    png_bytep *lines;
    FILE *outf;
    HDC hMemDC;
    BITMAPINFO bi;
    BITMAP bm;
    DWORD dwWidthBytes, cbBits;
    BYTE *pbBits;
    BOOL f;
    INT y;
    INT nDepth;

    if (GetObject(hbm, sizeof(BITMAP), &bm) != sizeof(BITMAP))
        return FALSE;

    nDepth = (bm.bmBitsPixel == 32 ? 32 : 24);
    dwWidthBytes = WIDTHBYTES(bm.bmWidth * nDepth);
    cbBits = dwWidthBytes * bm.bmHeight;
    pbBits = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cbBits);
    if (pbBits == NULL)
        return FALSE;

    f = FALSE;
    hMemDC = CreateCompatibleDC(NULL);
    if (hMemDC != NULL)
    {
        ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
        bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth    = bm.bmWidth;
        bi.bmiHeader.biHeight   = bm.bmHeight;
        bi.bmiHeader.biPlanes   = 1;
        bi.bmiHeader.biBitCount = (WORD)nDepth;
        f = GetDIBits(hMemDC, hbm, 0, bm.bmHeight, pbBits, &bi, 
                      DIB_RGB_COLORS);
        DeleteDC(hMemDC);
    }
    if (!f)
    {
        HeapFree(GetProcessHeap(), 0, pbBits);
        return FALSE;
    }

    outf = fopen(pszFileName, "wb");
    if (!outf)
    {
        HeapFree(GetProcessHeap(), 0, pbBits);
        return FALSE;
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL)
    {
        HeapFree(GetProcessHeap(), 0, pbBits);
        fclose(outf);
        DeleteFile(pszFileName);
        return FALSE;
    }

    info = png_create_info_struct(png);
    if (info == NULL)
    {
        HeapFree(GetProcessHeap(), 0, pbBits);
        png_destroy_write_struct(&png, png_infopp_NULL);
        fclose(outf);
        DeleteFile(pszFileName);
        return FALSE;
    }

    lines = NULL;
    if (setjmp(png_jmpbuf(png)))
    {
        HeapFree(GetProcessHeap(), 0, pbBits);
        if (lines != NULL)
            HeapFree(GetProcessHeap(), 0, lines);
        fclose(outf);
        DeleteFile(pszFileName);
        return FALSE;
    }

    png_init_io(png, outf);
    png_set_IHDR(png, info, bm.bmWidth, bm.bmHeight, 8, 
        (nDepth == 32 ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB),
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);

    sBIT.red = 8;
    sBIT.green = 8;
    sBIT.blue = 8;
    sBIT.alpha = (WORD)(nDepth == 32 ? 8 : 0);
    png_set_sBIT(png, info, &sBIT);

    if (dpi != 0.0)
    {
        png_uint_32 res = dpi * 100 / 2.54 + 0.5;
        png_set_pHYs(png, info, res, res, PNG_RESOLUTION_METER);
    }

    png_write_info(png, info);
    png_set_bgr(png);

    lines = (png_bytep *)HeapAlloc(GetProcessHeap(), 0, 
                                   sizeof(png_bytep *) * bm.bmHeight);
    for (y = 0; y < bm.bmHeight; y++)
        lines[y] = (png_bytep)&pbBits[dwWidthBytes * (bm.bmHeight - y - 1)];

    png_write_image(png, lines);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);

    HeapFree(GetProcessHeap(), 0, pbBits);
    HeapFree(GetProcessHeap(), 0, lines);
    fclose(outf);

    return TRUE;
}
