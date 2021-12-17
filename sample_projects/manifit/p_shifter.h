/***********************************************
//	p_shifter.h
//	version 1.0.2
//	1 December, 2021
//	Programmed by Kaoru Ashihara
//	Copyright (c) 2021 AIST
***********************************************/

/* Prototyes */
void initialize(HWND hWnd, int iLvl, int iRel, int iMrk, unsigned ch, BOOL flg);
BOOL genFunc(HWND hWnd);
BOOL prepFir(HWND hwnd, short sPitch);
BOOL prepFir(HWND hwnd);
BOOL genFir(HWND hwnd, short sParam);
DWORD convolve(HWND hWnd, LPSTR lpOrigi, LPSTR lpData, LPSTR lpBuf, DWORD dwOffset, DWORD dwUnt, short sCurrPit);
void finalize(HWND hWnd);

/* Global variables */
extern short sNumTaps;
extern BOOL bFlg;
extern BOOL isAllowed;