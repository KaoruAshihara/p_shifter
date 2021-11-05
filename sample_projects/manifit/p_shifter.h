/* Prototyes */
void initialize(HWND hWnd, int iLvl, int iRel, int iMrk, short sNo, short sAd,unsigned ch,BOOL flg);
void firNoSetter(HWND hWnd, short sNo);
void genFunc(HWND hWnd, short sTaps);
void genFir(HWND hwnd, int iParam,short sTap);
DWORD convolve(HWND hWnd, LPSTR lpOrigi, LPSTR lpData,LPSTR lpBuf, DWORD dwOffset, DWORD dwUnt,short sCurrPit,short sTap);
void finalize(HWND hWnd);