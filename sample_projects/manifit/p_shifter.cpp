/*********************************************************************
//	p_shifter.cpp
//	version 1.0.4
//	13 December, 2021
//	Programmed by Kaoru Ashihara
//	Copyright (c) 2021 AIST
//
//	Released under the MIT license
//	MIT license -> https://opensource.org/licenses/mit-license.php
*********************************************************************/
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include "math.h"
#include "p_shifter.h"		// Include the header file of your project

/* Global function prototyes and extern variables
The function prototypes and extern variables listed below must be declared in the header file */

/***********************************
// prototypes
void initialize(HWND hWnd, int iLvl, int iRel, int iMrk, unsigned ch, BOOL flg);
BOOL genFunc(HWND hWnd);
BOOL prepFir(HWND hwnd,short sPitch);
BOOL prepFir(HWND hwnd);
BOOL genFir(HWND hwnd, short sParam);
DWORD convolve(HWND hWnd, LPSTR lpOrigi, LPSTR lpData, LPSTR lpBuf, DWORD dwOffset, DWORD dwUnt, short sCurrPit);
void finalize(HWND hWnd);

// global variables
extern short sNumTaps;
extern BOOL bFlg;
extern BOOL isAllowed;
***********************************/

/* Functions */
static BOOL fastFt(HWND hwnd, double real[], double image[], BOOL isInv);
static int gcd(HWND hwnd, int x, int y);
static BOOL malocfir(HWND hWnd, LPSTR *lpFilter, DWORD dwSize);

/* Constants */
#define PI 3.14159265359
#define MAXVALUE 32767
const int iBytes = 2;
const double dWpi = PI * 2;
const float semi = powf(2.0F, 1.0F / 12.0F);

/* Global variables */
double *winFunc;					// Window function
unsigned Channels;					// 1 for mono, 2 for stereo
int iLev, iRelease, iElapsed;
int iQ, iO, iE;
short sNumTaps;						// FIR filter tap size
DWORD dwAdv, dwEnd, dwA, dwPosit;
DWORD *dwHead = new DWORD[73];
DWORD *dwTail = new DWORD[73];
BOOL isToRewind = false;
BOOL bFlg, isAllowed;
LPSTR lpEven;

/* Functions */
/***********************************
Initializer
Initialize the parameters
***********************************/
void initialize(HWND hWnd, int iThr, int iRel, int iMrk, unsigned ch, BOOL flg) {
	iLev = (int)(MAXVALUE * powf(10.0F, (float)iThr / 20.0F));	// Threshold
	iRelease = iRel;				// Latency in samples
	iElapsed = iMrk;				// This can be initialized with 0
	Channels = ch;					// 1 for mono, 2 for stereo
	isToRewind = flg;				// Initialize with FALSE
}

/************************************
Generate a window function
************************************/
BOOL genFunc(HWND hWnd) {
	double dRA = PI / 2.0;
	int iCnt;
	short sAns = sNumTaps & (sNumTaps - 1);

	if (sAns != 0)
		return(false);

	iQ = sNumTaps / 4;
	iO = sNumTaps / 8;
	iE = sNumTaps - iO;
	delete[] winFunc;
	winFunc = new double[sNumTaps];

	for (iCnt = 0;iCnt < sNumTaps;iCnt++) {
		if (iCnt < iO || iCnt >= iE)
			winFunc[iCnt] = 0;
		else if (iCnt < iO * 3)
			winFunc[iCnt] = sin(dRA * ((double)iCnt - (double)iO) / (double)iQ);
		else if (iCnt < iO * 5)
			winFunc[iCnt] = 1.0;
		else
			winFunc[iCnt] = sin(PI * ((double)iCnt - ((double)iO * 3)) / ((double)iO * 4));
	}
	return(true);
}

/***************************************************************
fast Fourier transform
real[] : Real part
image[] : Imaginary part
isInv : BOOL indicates FFT (false) or inverse FFT (true)
***************************************************************/
static BOOL fastFt(HWND hwnd, double real[], double image[], BOOL isInv) {
	int n = sNumTaps;
	double sc, f, c, s, t, c1, s1, x1, kyo1;
	int j, i, k, ns, l1, i0, i1;
	int iInt;

	/*	******************** Arranging BIT ****************** */

	sc = PI;
	j = 0;
	for (i = 0;i < n - 1;i++)
	{
		if (i <= j)
		{
			t = real[i];  real[i] = real[j];  real[j] = t;
			t = image[i];   image[i] = image[j];   image[j] = t;
		}
		k = n / 2;
		while (k <= j)
		{
			j = j - k;
			k /= 2;
		}
		j += k;
	}
	/*	******************** MAIN LOOP ********************** */
	ns = 1;
	if (isInv)															// inverse
		f = 1.0;
	else
		f = -1.0;
	while (ns <= n / 2)
	{
		c1 = cos(sc);
		s1 = sin(f * sc);
		c = 1.0;
		s = 0.0;
		for (l1 = 0;l1 < ns;l1++)
		{
			for (i0 = l1;i0 < n;i0 += (2 * ns))
			{
				i1 = i0 + ns;
				x1 = (real[i1] * c) - (image[i1] * s);
				kyo1 = (image[i1] * c) + (real[i1] * s);
				real[i1] = real[i0] - x1;
				image[i1] = image[i0] - kyo1;
				real[i0] = real[i0] + x1;
				image[i0] = image[i0] + kyo1;
			}
			t = (c1 * c) - (s1 * s);
			s = (s1 * c) + (c1 * s);
			c = t;
		}
		ns *= 2;
		sc /= 2.0;
	}

	if (!isInv) {
		for (iInt = 0;iInt < n;iInt++) {
			real[iInt] /= (double)n;
			image[iInt] /= (double)n;
		}
	}
	return(true);
}

/*********************************************************************
Great Common Divisor
This function returns the great common divisor of the integers x and y
*********************************************************************/
static int gcd(HWND hwnd, int x, int y) {
	x = abs(x);
	y = abs(y);
	while (y != 0) {
		int t = y;
		y = x % y;
		x = t;
	}
	return x;
}

/**********************************************************************
Alocate memory for FIR filter
sPitch : Pitch
**********************************************************************/
BOOL prepFir(HWND hwnd, short sPitch) {
	DWORD dwLen;
	int iSub;
	int iHlf = sNumTaps / 2;
	double ratio, div, dRate, dSub;

	if (sPitch == 0) {
		iSub = 1;
	}
	else {
		if (sPitch < 0) {
			ratio = pow(semi, (double)sPitch);
			div = 1.0 - ratio;
		}
		else {
			ratio = pow(semi, (double)sPitch);
			div = ratio - 1.0;
		}
		dRate = 1.0 / div;
		dSub = (double)iHlf * dRate;
		iSub = (int)round(dSub);
	}
	dwLen = (DWORD)sNumTaps * iSub;
	if (lpEven) {
		GlobalFreePtr(lpEven);
		lpEven = NULL;
	}
	if (!malocfir(hwnd, &lpEven, dwLen * (DWORD)iBytes)) {
		return(false);
	}
	dwPosit = 0;
	return(true);
}

/**********************************************************************
Alocate memory for FIR filter
dwLen : Data length
**********************************************************************/
BOOL prepFir(HWND hwnd) {
	DWORD dwLen;

	if (sNumTaps == 512)
		dwLen = 19755520;
	else if (sNumTaps == 1024)
		dwLen = 79029248;
	else if (sNumTaps == 2048)
		dwLen = 316102656;
	else
		dwLen = 1264472064;

	if (lpEven) {
		GlobalFreePtr(lpEven);
		lpEven = NULL;
	}
	if (!malocfir(hwnd, &lpEven, dwLen * (DWORD)iBytes)) {
		return(false);
	}
	dwPosit = 0;
	return(true);
}

/********************************************************************
FIR filter generator
sPrm : Pitch shift amount in semitone
********************************************************************/
BOOL genFir(HWND hwnd, short sPrm) {
	int i, f, iSub, iShift, iCutoff;
	int iHlf = sNumTaps / 2;
	short sParam;
	short *lpFir = (short *)lpEven;
	double dAmp, amp, phase, div, ratio, dRate, dSub;

	if (sPrm < 0)
		sParam = 36 - sPrm;
	else
		sParam = sPrm;

	if (sParam == 0) {
		dwHead[sParam] = dwPosit;
		iSub = 1;

		for (f = 0;f < sNumTaps;f++) {
			if (f == 0 || f == sNumTaps / 2)
				lpFir[dwPosit] = MAXVALUE;
			else
				lpFir[dwPosit] = 0;
			dwPosit++;
		}
		dwTail[sParam] = dwPosit;
	}
	else {
		dwHead[sParam] = dwPosit;
		dAmp = (64.0 * 1024.0 - 1.0) / (double)sNumTaps;

		double *re = new double[sNumTaps];
		double *im = new double[sNumTaps];

		if (sParam > 36) {
			ratio = pow(semi, 36 - (double)sParam);
			div = 1.0 - ratio;
		}
		else {
			ratio = pow(semi, (double)sParam);
			div = ratio - 1.0;
		}

		dRate = 1.0 / div;
		dSub = (double)iHlf * dRate;
		iSub = (int)round(dSub);
		iShift = iHlf;

		iCutoff = (int)((double)sNumTaps / (2.0 * ratio));

		for (i = 0;i < iSub;i++) {
			for (f = 0;f <= sNumTaps / 2;f++) {
				if (sParam <= 36 && f > iCutoff)
					amp = 0;
				else if (f % 2 != 0)
					amp = 0;
				else
					amp = dAmp;
				phase = (double)f * (double)iHlf * dWpi * (double)i / ((double)sNumTaps * (double)iSub);
				re[f] = amp * cos(phase);
				im[f] = amp * sin(phase);
				if (f > 0 && f < sNumTaps / 2) {
					re[sNumTaps - f] = amp * cos(-phase);
					im[sNumTaps - f] = amp * sin(-phase);
				}
			}
			fastFt(hwnd, re, im, true);
			for (f = 0;f < sNumTaps;f++) {
				if (sParam <= 36)
					lpFir[dwPosit] = (int)(re[f] * winFunc[f]);
				else
					lpFir[dwPosit] = (int)(re[sNumTaps - f - 1] * winFunc[sNumTaps - f - 1]);
				dwPosit++;
			}
		}
		dwTail[sParam] = dwPosit;

		/*
		FILE* file;
		if ((errno = fopen_s(&file, "example.txt", "w")) != 0) {
		fprintf(stderr, "Error: cannot open \"%s\".\n", "example.txt");
		exit(1);
		}
		for (i = 0;i < sNumTaps;i++)
		fprintf(file, "%d\n", iUp[1][i]);
		fclose(file);	*/

		delete[] re;
		delete[] im;

	}
	return(true);
}

/********************************************************************
Convolve the filter with the data
lpOrigi : Original data with which the filter is convolved
lpData : Data obtained by the processing
lpBuf : Buffer for the realtime reproduction
dwOffset : Current position in samples
dwUnt : Length of a single buffer in samples per channel
sCurrPitch : Pitch shift amount in semitone

This function returns the current position
(total samples per channel processed)
********************************************************************/
DWORD convolve(HWND hWnd, LPSTR lpOrigi, LPSTR lpData, LPSTR lpBuf, DWORD dwOffset, DWORD dwUnt, short sCurrPitch) {
	int iC, iTmp;
	short sNyq = sNumTaps / 2;
	short *lpPri = (short *)lpOrigi;
	short *lpRes = (short *)lpData;
	short *lpFir = (short *)lpEven;
	short *lpBuffer;
	short sParam;
	double dL, dR;
	DWORD dwCurr, dwK;
	DWORD dwCnt = 0;
	BOOL isRec = true;

	if (lpBuf) {
		lpBuffer = (short *)lpBuf;
		isRec = false;
	}
	else
		lpBuffer = NULL;

	if (sCurrPitch < 0)
		sParam = 36 - sCurrPitch;
	else
		sParam = sCurrPitch;

	if (bFlg) {
		dwAdv = dwHead[sParam];
		dwEnd = dwTail[sParam];

		dwA = dwAdv + iO;
		bFlg = false;
	}

	for (dwCurr = dwOffset;dwCurr < dwOffset + dwUnt;dwCurr++) {
		dL = dR = 0;
		for (iTmp = iO;iTmp < iE;iTmp++) {
			if ((int)dwCurr < iTmp)
				iC = 0;
			else {
				iC = (int)dwCurr - iTmp;
				iC *= Channels;
			}
			dL += (double)lpPri[iC] * (double)lpFir[dwA];
			if (Channels == 2)
				dR += (double)lpPri[iC + 1] * (double)lpFir[dwA];
			dwA++;
		}
		dwA += iQ;
		if (dwA >= dwEnd) {
			dwA = dwAdv + iO;
		}

		dwK = dwCurr * Channels;
		dL /= MAXVALUE;
		if (isRec) {
			lpRes[dwK] = (short)dL;
			if (Channels == 2) {
				dR /= MAXVALUE;
				lpRes[dwK + 1] = (short)dR;
			}
		}
		else {
			lpBuffer[dwCnt] = lpRes[dwK] = (short)dL;
			dwCnt++;
			if (Channels == 2) {
				dR /= MAXVALUE;
				lpBuffer[dwCnt] = lpRes[dwK + 1] = (short)dR;
				dwCnt++;
			}
		}

		if (lpPri[dwK] < iLev && lpPri[dwK + 1] < iLev) {
			iElapsed++;
			if (iElapsed > iRelease)
				isToRewind = true;
		}
		else {
			if (isToRewind) {
				dwAdv = dwHead[sParam];
				dwEnd = dwTail[sParam];
				dwA = dwAdv + iO;
				isToRewind = false;
			}
			iElapsed = 0;
		}
	}
	return(dwCurr);
}

BOOL malocfir(HWND hWnd, LPSTR *lpFilter, DWORD dwSize) {

	LPSTR lpData = (LPSTR)GlobalAllocPtr(GMEM_MOVEABLE, dwSize);
	if (!lpData) {
		return(FALSE);
	}
	*lpFilter = lpData;
	return(TRUE);
}

/********************************************************************
Finalizer
Delete the FIR filter arrays
Delete the window function
********************************************************************/
void finalize(HWND hWnd) {
	if (lpEven)
		GlobalFreePtr(lpEven);
	delete[] winFunc;
}
