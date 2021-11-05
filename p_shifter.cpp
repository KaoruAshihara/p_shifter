/*********************************************************************
//	p_shifter.cpp
//	version 1.0.1
//	5 November, 2021
//	Programmed by Kaoru Ashihara
//	Copyright (c) 2021 AIST
//
//	Released under the MIT license
//	MIT license -> https://opensource.org/licenses/mit-license.php
*********************************************************************/
#include <windows.h>
#include <windowsx.h>
#include "math.h"
#include "p_shifter.h"		// Include the header file of your project

/* Global function prototyes
The function prototypes listed below must be declared in the header file */
/***********************************
void initialize(HWND hWnd);
void firNoSetter(HWND hWnd, short sNo);
void genFunc(HWND hWnd, short sTap);
void genFir(HWND hwnd, int iParam, short sTap);
DWORD convolve(HWND hWnd, LPSTR lpOrigi, LPSTR lpData, LPSTR lpBuf, DWORD dwOffset, DWORD dwUnt, short sCurrPit, short sTap);
void finalize(HWND hWnd);
***********************************/

/* Functions */
static BOOL fastFt(HWND hwnd, double real[], double image[], short sTap, BOOL isInv);
static int gcd(HWND hwnd, int x, int y);

/* Constants */
#define PI 3.14159265359
#define MAXVALUE 32767
const double dWpi = PI * 2;
const float semi = powf(2.0F, 1.0F / 12.0F);

/* Global variables */
double *winFunc;					// Window function
unsigned Channels;					// 1 for mono, 2 for stereo
int iLev, iRelease, iElapsed;
int ***iUp = new int**[37];			// FIR filter for ascending pitch
int ***iDown = new int**[37];		// FIR filter for descending pitch
short sFirNo, sAdv;
short sNumUp[37] = {};
short sNumDown[37] = {};
short sShiftUp[37] = {};
short sShiftDown[37] = {};
BOOL isToRewind = false;

/* Functions */
/***********************************
Initializer
Initialize the parameters
***********************************/
void initialize(HWND hWnd, int iThr, int iRel, int iMrk, short sNo, short sAd, unsigned ch, BOOL flg) {
	iLev = (int)(MAXVALUE * powf(10.0F, (float)iThr / 20.0F));	// Threshold
	iRelease = iRel;				// Latency in samples
	iElapsed = iMrk;				// This can be initialized with 0
	sFirNo = sNo;					// This can be initialized with 0
	sAdv = sAd;						// This can be initialized with 0
	Channels = ch;					// 1 for mono, 2 for stereo
	isToRewind = flg;				// Initialize with FALSE
}

/**********************************
Parameter updater
**********************************/
void firNoSetter(HWND hWnd, short sNo) {
	sFirNo = sNo;
}

/************************************
Generate a window function
	sTap : The number of the FIR taps
************************************/
void genFunc(HWND hWnd, short sTap) {
	double dRA = PI / 2.0;
	int iQ, iO, iCnt;

	iQ = sTap / 4;
	iO = sTap / 8;
	delete[] winFunc;
	winFunc = new double[sTap];

	for (iCnt = 0;iCnt < sTap;iCnt++) {
		if (iCnt < iO || iCnt >= sTap - iO)
			winFunc[iCnt] = 0;
		else if (iCnt < iO * 3)
			winFunc[iCnt] = sin(dRA * ((double)iCnt - (double)iO) / (double)iQ);
		else if (iCnt < iO * 5)
			winFunc[iCnt] = 1.0;
		else
			winFunc[iCnt] = sin(PI * ((double)iCnt - ((double)iO * 3)) / ((double)iO * 4));
	}
}

/***************************************************************
fast Fourier transform
	real[] : Real part
	image[] : Imaginary part
	sTap : FFT window size
	isInv : BOOL indicates FFT (false) or inverse FFT (true)
***************************************************************/
static BOOL fastFt(HWND hwnd, double real[], double image[], short sTap, BOOL isInv) {
	int n = sTap;
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

/********************************************************************
FIR filter generator
	iParam : Pitch shift amount in semitone
	sTap : The number of the FIR taps
********************************************************************/
void genFir(HWND hwnd, int iParam, short sTap) {
	int i, f, perc, comdiv, iLen, iShift,iCutoff;
	double dAmp, amp, phase, amount,ratio;

	for (i = 0;i < sNumDown[iParam];i++) {
		delete[] iDown[iParam][i];
	}
	for (i = 0;i < sNumUp[iParam];i++) {
		delete[] iUp[iParam][i];
	}

	if (iParam == 0) {
		iLen = 1;
		iShift = 0;
		iUp[iParam] = new int*[iLen];
		iDown[iParam] = new int*[iLen];
		for (i = 0;i < iLen;i++) {
			iDown[iParam][i] = new int[sTap];
			iUp[iParam][i] = new int[sTap];
			for (f = 0;f < sTap;f++) {
				if (f == 0 || f == sTap / 2)
					iUp[iParam][i][f] = MAXVALUE;
				else
					iUp[iParam][i][f] = 0;
				iDown[iParam][i][f] = 0;
			}
		}
		sNumUp[iParam] = iLen;
		sNumDown[iParam] = iLen;
		sShiftUp[iParam] = iShift;
		sShiftDown[iParam] = iShift;
	}
	else {
		dAmp = (64.0 * 1024.0 - 1.0) / (double)sTap;

		double *re = new double[sTap];
		double *im = new double[sTap];

		ratio = pow(semi, (double)iParam);
		amount = ratio - 1.0;
		perc = (int)round(amount * 200.0);
		comdiv = gcd(hwnd, 200, perc);
		iLen = 200 / comdiv;
		iShift = perc / comdiv;

		iCutoff = (int)((double)sTap / (2.0 * ratio));
		iUp[iParam] = new int*[iLen];

		for (i = 0;i < iLen;i++) {
			iUp[iParam][i] = new int[sTap];
			for (f = 0;f <= sTap / 2;f++) {
				if (f > iCutoff || f % 2 != 0)
					amp = 0;
				else
					amp = dAmp;
				phase = (double)f * (double)iShift * dWpi * (double)i / ((double)sTap * (double)iLen);
				re[f] = amp * cos(phase);
				im[f] = amp * sin(phase);
				if (f > 0 && f < sTap / 2) {
					re[sTap - f] = amp * cos(-phase);
					im[sTap - f] = amp * sin(-phase);
				}
			}
			fastFt(hwnd, re, im, sTap, true);
			for (f = 0;f < sTap;f++) {
				iUp[iParam][i][f] = (int)re[f];
			}
		}
		sNumUp[iParam] = iLen;
		sShiftUp[iParam] = iShift;
		delete[] re;
		delete[] im;

		double *re1 = new double[sTap];
		double *im1 = new double[sTap];
		amount = 1.0 - pow(semi, (double)-iParam);
		perc = (int)round(amount * 200.0);
		comdiv = gcd(hwnd, 200, perc);
		iLen = 200 / comdiv;
		iShift = perc / comdiv;
		iDown[iParam] = new int*[iLen];

		for (i = 0;i < iLen;i++) {
			iDown[iParam][i] = new int[sTap];
			for (f = 0;f <= sTap / 2;f++) {
				if (f % 2 == 0)
					amp = dAmp;
				else
					amp = 0;
				phase = (double)f * (double)iShift * dWpi * (double)i / ((double)sTap * (double)iLen);
				re1[f] = amp * cos(phase);
				im1[f] = amp * sin(phase);
				if (f > 0 && f < sTap / 2) {
					re1[sTap - f] = amp * cos(-phase);
					im1[sTap - f] = amp * sin(-phase);
				}
			}
			fastFt(hwnd, re1, im1, sTap, true);
			for (f = 0;f < sTap;f++) {
				iDown[iParam][i][sTap - f - 1] = (int)re1[f];
			}
		}
		sNumDown[iParam] = iLen;
		sShiftDown[iParam] = iShift;
		delete[] re1;
		delete[] im1;
	}
}

/********************************************************************
Convolve the filter with the data
	lpOrigi : Original data with which the filter is convolved
	lpData : Data obtained by the processing
	lpBuf : Buffer for the realtime reproduction
	dwOffset : Current position in samples
	dwUnt : Length of a single buffer in samples per channel
	sCurrPitch : Pitch shift amount in semitone
	sTap : The number of the FIR taps

This function returns the current position
(total samples per channel processed)
********************************************************************/
DWORD convolve(HWND hWnd, LPSTR lpOrigi, LPSTR lpData, LPSTR lpBuf, DWORD dwOffset, DWORD dwUnt, short sCurrPitch, short sTap) {
	int c, iC, iD, iTmp;
	short sNyq = sTap / 2;
	short *lpPri = (short *)lpOrigi;
	short *lpRes = (short *)lpData;
	short *lpBuffer;
	double dL, dR;
	DWORD dwCurr;
	DWORD dwCnt = 0;
	BOOL isRec = true;

	if (lpBuf) {
		lpBuffer = (short *)lpBuf;
		isRec = false;
	}
	else
		lpBuffer = NULL;

	for (dwCurr = dwOffset;dwCurr < dwOffset + dwUnt;dwCurr++) {
		for (c = 0;c < (int)Channels;c++) {
			dL = dR = 0;
			for (iTmp = 0;iTmp < (int)sTap;iTmp++) {
				iD = (int)sAdv + iTmp;
				if (iD >= (int)sTap)
					iD -= (int)sTap;
				else if (iD < 0)
					iD += (int)sTap;

				if ((int)dwCurr < iTmp)
					iC = 0;
				else
					iC = (int)dwCurr - iTmp;

				if (c == 0) {
					if (sCurrPitch < 0)
						dL += (double)lpPri[iC * Channels] * (double)iDown[-sCurrPitch][sFirNo][iD] * winFunc[iTmp];
					else
						dL += (double)lpPri[iC * Channels] * (double)iUp[sCurrPitch][sFirNo][iD] * winFunc[iTmp];
				}
				else {
					if (sCurrPitch < 0)
						dR += (double)lpPri[iC * Channels + c] * (double)iDown[-sCurrPitch][sFirNo][iD] * winFunc[iTmp];
					else
						dR += (double)lpPri[iC * Channels + c] * (double)iUp[sCurrPitch][sFirNo][iD] * winFunc[iTmp];
				}
			}
			if (c == 0) {
				dL /= MAXVALUE;
				if (isRec)
					lpRes[dwCurr * Channels] = (short)dL;
				else
					lpBuffer[dwCnt] = lpRes[dwCurr * Channels] = (short)dL;
			}
			else {
				dR /= MAXVALUE;
				if (isRec)
					lpRes[dwCurr * Channels + c] = (short)dR;
				else
					lpBuffer[dwCnt] = lpRes[dwCurr * Channels + c] = (short)dR;
			}
			dwCnt++;
			if (c == Channels - 1) {
				if (sCurrPitch != 0)
					sFirNo++;
				if (sCurrPitch > 0) {
					if (sFirNo >= sNumUp[sCurrPitch]) {
						sAdv += sShiftUp[sCurrPitch];
						if (sAdv >= sNyq)
							sAdv -= sNyq;
						sFirNo = 0;
					}
				}
				else if (sCurrPitch < 0) {
					if (sFirNo >= sNumDown[-sCurrPitch]) {
						sAdv -= sShiftDown[-sCurrPitch];
						if (sAdv <= -sNyq)
							sAdv += sNyq;
						sFirNo = 0;
					}
				}
				if (lpPri[dwCurr * Channels] < iLev && lpPri[dwCurr * Channels + 1] < iLev) {
					iElapsed++;
					if (iElapsed > iRelease)
						isToRewind = true;
				}
				else {
					if (isToRewind) {
						sAdv = 0;
						isToRewind = false;
					}
					iElapsed = 0;
				}
			}
		}
	}
	return(dwCurr);
}

/********************************************************************
Finalizer
Delete the FIR filter arrays
Delete the window function
********************************************************************/
void finalize(HWND hWnd) {
	int iCnt, i;
	for (iCnt = 0;iCnt < 37;iCnt++) {
		for (i = 0;i < sNumDown[iCnt];i++) {
			delete[] iDown[iCnt][i];
		}
		for (i = 0;i < sNumUp[iCnt];i++) {
			delete[] iUp[iCnt][i];
		}
	}
	delete[] winFunc;
}
