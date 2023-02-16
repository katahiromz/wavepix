#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#define XMD_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <jpeglib.h>
#include <jerror.h>
#ifdef __cplusplus
}
#endif

#pragma comment(lib, "libjpeg.lib")

HBITMAP LoadJpegAsBitmap(LPCSTR pszFileName, float *dpi)
{
    FILE *fp;
    struct jpeg_decompress_struct decomp;
    struct jpeg_error_mgr jerror;
    BITMAPINFO bi;
    BYTE *lpBuf, *pb;
    HBITMAP hbm;
    JSAMPARRAY buffer;
    INT row;

    fp = fopen(pszFileName, "rb");
    if (fp == NULL)
        return NULL;

    decomp.err = jpeg_std_error(&jerror);

    jpeg_create_decompress(&decomp);
    jpeg_stdio_src(&decomp, fp);

    jpeg_read_header(&decomp, TRUE);
    jpeg_start_decompress(&decomp);
    switch(decomp.density_unit)
    {
    case 1: /* dots/inch */
        *dpi = decomp.X_density;
        break;

    case 2: /* dots/cm */
        *dpi = decomp.X_density * 2.54;
        break;

    default:
        *dpi = 0.0;
    }

    row = ((decomp.output_width * 3 + 3) & ~3);
    buffer = (*decomp.mem->alloc_sarray)((j_common_ptr)&decomp, JPOOL_IMAGE,
                                         row, 1);

    ZeroMemory(&bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth        = decomp.output_width;
    bi.bmiHeader.biHeight       = decomp.output_height;
    bi.bmiHeader.biPlanes       = 1;
    bi.bmiHeader.biBitCount     = 24;
    bi.bmiHeader.biCompression  = BI_RGB;
    bi.bmiHeader.biSizeImage    = row * decomp.output_height;

    hbm = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&lpBuf, NULL, 0);
    if (hbm == NULL)
    {
        jpeg_destroy_decompress(&decomp);
        fclose(fp);
        return NULL;
    }

    pb = lpBuf + row * decomp.output_height;
    while(decomp.output_scanline < decomp.output_height)
    {
        pb -= row;
        jpeg_read_scanlines(&decomp, buffer, 1);

        if (decomp.out_color_components == 1)
        {
            UINT i;
            BYTE *p = (BYTE *)buffer[0];
            for(i = 0; i < decomp.output_width; i++)
            {
                pb[3 * i + 0] = p[i];
                pb[3 * i + 1] = p[i];
                pb[3 * i + 2] = p[i];
            }
        }
        else if (decomp.out_color_components == 3)
        {
            CopyMemory(pb, buffer[0], row);
        }
        else
        {
            jpeg_destroy_decompress(&decomp);
            fclose(fp);
            DeleteObject(hbm);
            return NULL;
        }
    }

    SetDIBits(NULL, hbm, 0, decomp.output_height, lpBuf, &bi, DIB_RGB_COLORS);

    jpeg_finish_decompress(&decomp);
    jpeg_destroy_decompress(&decomp);

    fclose(fp);

    return hbm;
}

BOOL SaveBitmapAsJpeg(LPCSTR pszFileName, HBITMAP hbm, INT quality, BOOL progression, float dpi)
{
    FILE *fp;
    BITMAP bm;
    struct jpeg_compress_struct comp;
    struct jpeg_error_mgr jerr;
    JSAMPLE * image_buffer;
    BITMAPINFO bi;
    HDC hDC, hMemDC;
    BYTE *pbBits;
    INT nWidthBytes;
    DWORD cbBits;
    BOOL f;

    if (GetObject(hbm, sizeof(BITMAP), &bm) != sizeof(BITMAP))
        return FALSE;

    fp = fopen(pszFileName, "wb");
    if (fp == NULL)
        return FALSE;

    comp.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&comp);
    jpeg_stdio_dest(&comp, fp);

    comp.image_width  = bm.bmWidth;
    comp.image_height = bm.bmHeight;
    comp.input_components = 3;
    comp.in_color_space = JCS_RGB;
    jpeg_set_defaults(&comp);
    if (dpi != 0.0)
    {
        comp.density_unit = 1; /* dots/inch */
        comp.X_density = comp.Y_density = dpi + 0.5;
    }
    jpeg_set_quality(&comp, quality, TRUE);
    if (progression)
        jpeg_simple_progression(&comp);

    jpeg_start_compress(&comp, TRUE);
    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = bm.bmWidth;
    bi.bmiHeader.biHeight   = bm.bmHeight;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 24;

    f = FALSE;
#define WIDTHBYTES(i) (((i) + 31) / 32 * 4)
    nWidthBytes = WIDTHBYTES(bm.bmWidth * 24);
    cbBits = nWidthBytes * bm.bmHeight;
    pbBits = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cbBits);
    if (pbBits != NULL)
    {
        image_buffer = (JSAMPLE *)HeapAlloc(GetProcessHeap(), 0, nWidthBytes);
        if (image_buffer != NULL)
        {
            hDC = GetDC(NULL);
            if (hDC != NULL)
            {
                hMemDC = CreateCompatibleDC(hDC);
                if (hMemDC != NULL)
                {
                    f = GetDIBits(hMemDC, hbm, 0, bm.bmHeight, pbBits,
                                  (BITMAPINFO*)&bi, DIB_RGB_COLORS);
                    DeleteDC(hMemDC);
                }
                ReleaseDC(NULL, hDC);
            }
            if (f)
            {
                INT x, y;
                BYTE *src, *dest;
                for(y = 0; y < bm.bmHeight; y++)
                {
                    dest = image_buffer;
                    src = &pbBits[(bm.bmHeight - y - 1) * nWidthBytes];
                    for(x = 0; x < bm.bmWidth; x++)
                    {
                        *dest++ = *src++;
                        *dest++ = *src++;
                        *dest++ = *src++;
                    }
                    jpeg_write_scanlines(&comp, &image_buffer, 1);
                }
            }
            HeapFree(GetProcessHeap(), 0, image_buffer);
        }
        HeapFree(GetProcessHeap(), 0, pbBits);
    }

    jpeg_finish_compress(&comp);
    jpeg_destroy_compress(&comp);

    fclose(fp);
    if (!f)
    {
        DeleteFile(pszFileName);
        return FALSE;
    }
    return TRUE;
}
