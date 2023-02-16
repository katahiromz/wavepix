HBITMAP CreateWaved32BppBitmap(
    HBITMAP hbmSrc, 
    double amplitude, 
    double length, 
    double phase, 
    BOOL fVertical);
HBITMAP LoadBitmapFromFile(LPCTSTR pszFileName, float *dpi);
BOOL SaveBitmapToFile(LPCTSTR pszFileName, HBITMAP hbm, float dpi);
HBITMAP CopyBitmap(HBITMAP hbm);
HBITMAP CreateSolid32BppBitmap(INT cx, INT cy, COLORREF clr);
BOOL IsDIBOpaque(HBITMAP hbm);
BOOL AlphaBlendBitmap(HBITMAP hbm1, HBITMAP hbm2);
