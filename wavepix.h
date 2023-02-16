enum
{
    BMP,
    GIF,
    JPEG,
    PNG,
    TIFF
};

#define MAX_ZOOM_PERCENT 800
#define MIN_ZOOM_PERCENT 10

/* bitmap.c */
#include "bitmap.h"

/* gif.c */
HBITMAP LoadGifAsBitmap(LPCSTR pszFileName, COLORREF *prgbTransparent);
BOOL Save32BppBitmapAsGif(LPCSTR pszFileName, HBITMAP hbm, COLORREF clrTransparent);

/* jpeg.c */
HBITMAP LoadJpegAsBitmap(LPCSTR pszFileName, float *dpi);
BOOL SaveBitmapAsJpeg(LPCSTR pszFileName, HBITMAP hbm, float quality, BOOL progression, float dpi);

/* png.c */
HBITMAP LoadPngAsBitmap(LPCSTR pszFileName, float *dpi);
BOOL SaveBitmapAsPngFile(LPCSTR pszFileName, HBITMAP hbm, float dpi);

/* tiff.c */
HBITMAP LoadTiffAsBitmap(LPCSTR pszFileName, float *dpi);
BOOL SaveBitmapAsTiff(LPCSTR pszFileName, HBITMAP hbm, float dpi);
