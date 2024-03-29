/***********************************************
//	manipit.cpp
//	version 1.0.6
//	17 August, 2022
//	Programmed by Kaoru Ashihara
//	Copyright (c) 2022 AIST
***********************************************/
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>            
#include <mmsystem.h>
#include <string>
#include <vector>
#include "resource.h"
#include "p_shifter.h"

#define BUFSIZE 8192
#define CHANNELS 2
#define BITDEPTH 16
#define MAXRECSEC 1800

/*********************************
PROTOTYPE
*********************************/
int WINAPI WinMain(HINSTANCE hinstThis, HINSTANCE hinstPrev, LPSTR lpszCmdLine, int iCmdShow);
static void FreeGlobalData(void);
static BOOL prepare(HWND hWnd);
static BOOL GetFileName(HWND hDlg, BOOL bOpenName, LPSTR lpszFile, int iMaxFileNmLen, LPSTR lpszFileTitle, int iMaxFileTitleLen);
static BOOL SaveWaveFile(HWND hDlg, LPSTR lpData, DWORD dwDataSize);
BOOL WriteWaveData(HWND hWnd, LPSTR lpszFileName, LPSTR lplpWaveData, DWORD dwdwWaveDataSize, DWORD dwdwSamplesPerSec);
BOOL memolocate(HWND hWnd, LPSTR *lpWavData, DWORD dwSize);
static void ReportError(HWND hWnd, int iErrorID);

/*********************************
Globl variables
*********************************/
PCMWAVEFORMAT pcmWaveFormat;
LPSTR lpOriginal;
LPSTR lpWaveData;
DWORD dwMaxSize;
DWORD dwRecCnt;
DWORD dwPos;
DWORD dwUnit, dwProc;
DWORD dwLength;
DWORD dwSrate;
short sPitch;
short sStopCnt;
static int iThr, iLat;
static int iMark, iRelease;
static short sAdv;
static char szOFNDefExt[] = "WAV";								// File extension
static char *szOFNFilter[] = { "Sound Files (*.WAV)\0 *.WAV\0All Files (*.*)\0    *.*\0\0" };
static char szFileTitle[_MAX_FNAME];
static char szmanipitClass[] = "manipitClass";
static WAVEFORMATEX wFormat;
static HWAVEOUT hOut;
static HWAVEIN hIn;
static WAVEHDR whdr0, whdr1, whdr;
static BYTE *byteBuf0, *byteBuf1, *dataBuf, *bTmp;
static BOOL isCapturing;
static BOOL isToStop = false;
static BOOL isMsgOn = false;

/*********************************
Dialogue Procedures
*********************************/
BOOL WINAPI About_DlgProc(
	HWND hDlg,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam) {

	switch (uMsg) {
	case WM_INITDIALOG:
		return(TRUE);

	case WM_COMMAND:
		switch (wParam) {
		case IDOK:
			EndDialog(hDlg, 0);
			return(TRUE);
		}

	default:
		return(FALSE);
	}

	UNREFERENCED_PARAMETER(uMsg);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
}

BOOL WINAPI Adv_DlgProc(
	HWND hDlg,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam) {
	HWND hwndCombo = GetDlgItem(hDlg, IDC_COMBO_TAPS);
	std::string str;
	char charry[255];
	BOOL bl;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDlgItemInt(hDlg, IDC_EDIT_THR, iThr, TRUE);
		SetDlgItemInt(hDlg, IDC_EDIT_LATENCY, iLat, FALSE);

		return(true);

	case WM_COMMAND:
		switch (wParam) {
		case IDC_EDIT_THR:
			break;

		case IDC_EDIT_LATENCY:
			break;

		case IDOK:
			iThr = GetDlgItemInt(hDlg, IDC_EDIT_THR, &bl, TRUE);
			if (iThr > 0) {
				str = "Threshold cannot exceed 0 dB!";
				strcpy_s(charry, sizeof(charry), str.c_str());
				MessageBox(hDlg, (LPSTR)charry, TEXT("Wrong value"), MB_ICONEXCLAMATION | MB_OK);
				iThr = 0;
				SetDlgItemInt(hDlg, IDC_EDIT_THR, iThr, TRUE);
				break;
			}

			iLat = GetDlgItemInt(hDlg, IDC_EDIT_LATENCY, &bl, TRUE);
			if (iLat < 0) {
				str = "Latency must be larger than 0 ms!";
				strcpy_s(charry, sizeof(charry), str.c_str());
				MessageBox(hDlg, (LPSTR)charry, TEXT("Wrong value"), MB_ICONEXCLAMATION | MB_OK);
				iLat = 0;
				SetDlgItemInt(hDlg, IDC_EDIT_LATENCY, iLat, FALSE);
				break;
			}
			iRelease = (int)((float)dwSrate * (float)iLat / 1000.0F);

			EndDialog(hDlg, TRUE);
			return (TRUE);
		}
		break;
	}
	return(false);
}

LRESULT WINAPI manipit_WndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	MMRESULT rtrn;
	HWND hwndCombo0 = GetDlgItem(hDlg, IDC_COMBO_RATE);
	HWND hwndCombo1 = GetDlgItem(hDlg, IDC_COMBO_TAPS);
	HWND hCheck = GetDlgItem(hDlg, IDC_CHECK);
	HMENU hMenu = GetMenu(hDlg);
	RECT rc;
	static DWORD dwCount;
	static int iSel, iCnt, i, j;
	int iX, iY;
	char ch[255];
	char charry[255];
	std::string str;
	std::vector<std::string> strRate =
	{ "8000","11025","16000","22050","24000","32000","44100","48000","88200","96000" };
	std::vector<std::string> strTaps = { "256","512","1024","2048","4096"," " };
	std::vector<std::string> strAmount =
	{ "36","35","34","33","32","31","30","29","28","27","26","25","24","23","22","21",
		"20","19","18","17","16","15","14","13","12","11","10","9","8","7","6","5","4","3","2","1","0",
		"-1","-2","-3","-4","-5","-6","-7","-8","-9","-10","-11","-12","-13","-14","-15","-16","-17","-18",
		"-19","-20","-21","-22","-23","-24","-25","-26","-27","-28","-29","-30",
		"-31","-32","-33","-34","-35","-36" };

	switch (msg) {
	case WM_INITDIALOG:
		iX = GetSystemMetrics(SM_CXSCREEN) / 2;
		iY = GetSystemMetrics(SM_CYSCREEN) / 3;
		GetClientRect(hDlg, &rc);
		iX -= rc.right / 2;
		iY -= rc.bottom / 2;
		SetWindowPos(hDlg, NULL, iX, iY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

		isCapturing = false;
		Static_SetText(GetDlgItem(hDlg, IDC_MSG), "HELLO THERE!");

		SendMessage(hwndCombo0, CB_RESETCONTENT, 0, 0L);
		for (iCnt = 0;iCnt < (int)strRate.size();iCnt++)
			SendMessage(hwndCombo0, CB_ADDSTRING, 0, (LPARAM)strRate[iCnt].c_str());
		SendMessage(hwndCombo0, CB_SETCURSEL, 6, (LPARAM)0);
		dwSrate = (DWORD)std::stoi(strRate[6]);

		SendMessage(hwndCombo1, CB_RESETCONTENT, 0, 0L);
		for (iCnt = 0;iCnt < (int)strTaps.size();iCnt++)
			SendMessage(hwndCombo1, CB_ADDSTRING, 0, (LPARAM)strTaps[iCnt].c_str());
		SendMessage(hwndCombo1, CB_SETCURSEL, 5, (LPARAM)0);
		sNumTaps = 0;

		SendMessage(GetDlgItem(hDlg, IDC_SPIN), UDM_SETRANGE, (WPARAM)0, (LPARAM)MAKELONG(36, -36));
		sPitch = 0;
		SendMessage(GetDlgItem(hDlg, IDC_SPIN), UDM_SETPOS, 0, (LPARAM)sPitch);

		iThr = 0;
		iLat = 1000;
		isAllowed = true;

		iRelease = (int)((float)dwSrate * (float)iLat / 1000.0F);

		if (isAllowed) {
			SendMessage(hCheck, BM_SETCHECK, 0, 0L);
		}
		else {
			SendMessage(hCheck, BM_SETCHECK, 1, 0L);
		}

		Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Select FIR tap number!");
		return 0;

	case WM_DESTROY:
		finalize(hDlg);
		FreeGlobalData();
		PostQuitMessage(0);
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_FILE_ORIGINAL:
			if (!SaveWaveFile(hDlg, lpOriginal, dwLength)) {
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Failed!");
			}
			else {
				strcpy_s(charry, sizeof(charry), "Saved to ");
				strcat_s(charry, sizeof(charry), szFileTitle);
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);
			}
			break;

		case ID_FILE_PROCESSED:
			if (!SaveWaveFile(hDlg, lpWaveData, dwLength)) {
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Failed!");
			}
			else {
				strcpy_s(charry, sizeof(charry), "Saved to ");
				strcat_s(charry, sizeof(charry), szFileTitle);
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);
			}
			break;

		case IDM_ABOUT:
			DialogBox(GetWindowInstance(hDlg),
				MAKEINTRESOURCE(DLG_ABOUT), hDlg, About_DlgProc);
			break;

		case IDM_ADVANCED:
			DialogBox(GetWindowInstance(hDlg),
				MAKEINTRESOURCE(DLG_ADVANCED), hDlg, Adv_DlgProc);
			break;

		case IDM_EXIT:
			FORWARD_WM_CLOSE(hDlg, PostMessage);
			break;

		case IDC_COMBO_RATE:
			iSel = (WORD)SendMessage(hwndCombo0, CB_GETCURSEL, 0, 0L);
			dwSrate = (DWORD)std::stoi(strRate[iSel]);
			break;

		case IDC_COMBO_TAPS:
			Static_SetText(GetDlgItem(hDlg, IDC_PERCENT), "");
			Static_SetText(GetDlgItem(hDlg, IDC_PROGRESS), "");
			if (isMsgOn)
				return(0);
			iSel = (WORD)SendMessage(hwndCombo1, CB_GETCURSEL, 0, 0L);
			if (iSel == 5)
				break;

			if (iSel == 4 && isAllowed && sNumTaps != 4096) {
				isMsgOn = true;
				i = MessageBox(hDlg, TEXT("2.529 GiB memory required!"), TEXT("Attention!"), MB_OKCANCEL | MB_ICONWARNING);
				if (i != IDOK) {
					if (sNumTaps == 256)
						iCnt = 0;
					else if (sNumTaps == 512)
						iCnt = 1;
					else if (sNumTaps == 1024)
						iCnt = 2;
					else if (sNumTaps == 2048)
						iCnt = 3;
					else if (sNumTaps == 4096)
						iCnt = 4;
					else
						iCnt = 5;

					isMsgOn = false;
					SendMessage(hwndCombo1, CB_SETCURSEL, iCnt, (LPARAM)0);
				}
				else {
					isMsgOn = false;
					Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Preparing filter. Just a moment, please");
					sNumTaps = 4096;
					EnableWindow(GetDlgItem(hDlg, IDB_REC), false);
					/* Prepare the window function */
					if(!genFunc(hDlg))
						Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Window function error!");
					else {
						/* Generate FIR */
						if (prepare(hDlg)) {
							Static_SetText(GetDlgItem(hDlg, IDC_MSG), "I am ready!");
							EnableWindow(GetDlgItem(hDlg, IDB_REC), true);
						}
					}
				}
			}
			else {
				isMsgOn = false;
				dwCount = (DWORD)std::stoi(strTaps[iSel]);
				if (dwCount != sNumTaps) {
					Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Preparing filter. Just a moment, please");
					sNumTaps = (short)dwCount;
					EnableWindow(GetDlgItem(hDlg, IDB_REC), false);
					/* Prepare the window function */
					if (!genFunc(hDlg))
						Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Window function error!");
					else {
						/* Generate FIR */
						if (prepare(hDlg)) {
							Static_SetText(GetDlgItem(hDlg, IDC_MSG), "I am ready!");
							EnableWindow(GetDlgItem(hDlg, IDB_REC), true);
						}
					}
				}
			}
			break;

		case IDC_EDIT_PITCH:
			if (!isAllowed && sNumTaps == 0) {
				sPitch = GetDlgItemInt(hDlg, IDC_EDIT_PITCH, false, true);
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Please select FIR tap number, first!");
			}
			else if(GetDlgItemInt(hDlg, IDC_EDIT_PITCH, false, true) != sPitch){
				sPitch = GetDlgItemInt(hDlg, IDC_EDIT_PITCH, false, true);
				bFlg = true;
				if (!isAllowed) {
					EnableWindow(GetDlgItem(hDlg, IDB_REC), false);
					Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Preparing filter. Just a moment, please");
					if (prepare(hDlg)) {
						Static_SetText(GetDlgItem(hDlg, IDC_MSG), "I am ready!");
						EnableWindow(GetDlgItem(hDlg, IDB_REC), true);
					}
				}
			}
			break;

		case IDC_CHECK:
			if (isAllowed) {
				isAllowed = FALSE;
			}
			else
				isAllowed = TRUE;
			EnableWindow(GetDlgItem(hDlg, IDB_REC), false);
			sNumTaps = 0;
			SendMessage(hwndCombo1, CB_SETCURSEL, 5, (LPARAM)0);
			Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Select FIR tap number!");
			break;

		case IDB_REC:
			if (isCapturing) {
				isToStop = TRUE;
			}
			else {
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK), false);
				EnableWindow(GetDlgItem(hDlg, IDC_COMBO_RATE), false);
				EnableWindow(GetDlgItem(hDlg, IDC_COMBO_TAPS), false);
				EnableMenuItem(hMenu, ID_FILE_ORIGINAL, MF_GRAYED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, ID_FILE_PROCESSED, MF_GRAYED | MF_BYCOMMAND);
				if (!isAllowed)
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PITCH), false);

				byteBuf0 = (BYTE*)malloc(BUFSIZE);
				byteBuf1 = (BYTE*)malloc(BUFSIZE);

				wFormat.wFormatTag = WAVE_FORMAT_PCM;
				wFormat.nChannels = CHANNELS;
				wFormat.nSamplesPerSec = dwSrate;
				wFormat.nAvgBytesPerSec = dwSrate * CHANNELS * BITDEPTH / 8;
				wFormat.wBitsPerSample = BITDEPTH;
				wFormat.nBlockAlign = wFormat.nChannels * wFormat.wBitsPerSample / 8;
				wFormat.cbSize = 0;

				dwMaxSize = wFormat.nAvgBytesPerSec * MAXRECSEC;
				dwUnit = BUFSIZE / (int)wFormat.nBlockAlign;

				whdr0.lpData = (LPSTR)byteBuf0;
				whdr0.dwBufferLength = BUFSIZE;
				whdr0.dwBytesRecorded = 0;
				whdr0.dwFlags = 0;
				whdr0.dwLoops = 1;
				whdr0.lpNext = NULL;
				whdr0.dwUser = 0;
				whdr0.reserved = 0;

				whdr1.lpData = (LPSTR)byteBuf1;
				whdr1.dwBufferLength = BUFSIZE;
				whdr1.dwBytesRecorded = 0;
				whdr1.dwFlags = 0;
				whdr1.dwLoops = 1;
				whdr1.lpNext = NULL;
				whdr1.dwUser = 0;
				whdr1.reserved = 0;

				if (lpWaveData) {
					GlobalFreePtr(lpWaveData);
					lpWaveData = NULL;
				}
				memolocate(hDlg, &lpWaveData, dwMaxSize);

				whdr.lpData = lpWaveData;
				whdr.dwBufferLength = dwMaxSize;
				whdr.dwBytesRecorded = 0;
				whdr.dwFlags = 0;
				whdr.dwLoops = 1;
				whdr.lpNext = NULL;
				whdr.dwUser = 0;
				whdr.reserved = 0;

				dwRecCnt = 0;
				dwPos = 0;
				sStopCnt = 0;
				iMark = 0;
				isToStop = false;
				isCapturing = true;
				bFlg = true;

				initialize(hDlg, iThr, iRelease, iMark, CHANNELS, false);

				EnableMenuItem(hMenu, IDM_ABOUT, MF_DISABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, IDM_ADVANCED, MF_DISABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, IDM_EXIT, MF_DISABLED | MF_BYCOMMAND);

				waveInOpen(&hIn, WAVE_MAPPER, &wFormat,
					(DWORD)hDlg, 0, CALLBACK_WINDOW);
				rtrn = waveInPrepareHeader(hIn, &whdr0, sizeof(WAVEHDR));
				if (rtrn != MMSYSERR_NOERROR) {
					waveInUnprepareHeader(hIn, &whdr0, sizeof(WAVEHDR));
					waveInClose(hIn);
					waveOutGetErrorText(rtrn, ch, 255);
					MessageBox(hDlg, ch, "Message Box1", MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
				rtrn = waveInPrepareHeader(hIn, &whdr1, sizeof(WAVEHDR));
				if (rtrn != MMSYSERR_NOERROR) {
					waveInUnprepareHeader(hIn, &whdr1, sizeof(WAVEHDR));
					waveInClose(hIn);
					waveOutGetErrorText(rtrn, ch, 255);
					MessageBox(hDlg, ch, "Message Box2", MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
			}
			break;

		}
		return 0;
	case MM_WIM_OPEN:
		dwLength = 0;
		dataBuf = (BYTE*)realloc(dataBuf, 1);

		Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Give me sounds!");
		Static_SetText(GetDlgItem(hDlg, IDB_REC), "STOP");
		waveInAddBuffer(hIn, &whdr0, sizeof(WAVEHDR));
		waveInAddBuffer(hIn, &whdr1, sizeof(WAVEHDR));
		waveInStart(hIn);
		return 0;

	case MM_WIM_DATA:
		dwRecCnt++;
		if (sStopCnt) {
			waveInClose(hIn);
			return 0;
		}
		else if (isToStop) {
			sStopCnt++;
			waveOutReset(hOut);
			waveInReset(hIn);
			isToStop = false;
			return 0;
		}

		bTmp = (BYTE*)realloc(dataBuf,
			dwLength + ((PWAVEHDR)lParam)->dwBytesRecorded);
		if (!bTmp) {
			isToStop = true;
			return 0;
		}
		dataBuf = bTmp;
		for (dwCount = 0; dwCount < ((PWAVEHDR)lParam)->dwBytesRecorded; dwCount++)
			*(dataBuf + dwLength + dwCount) = *(((PWAVEHDR)lParam)->lpData + dwCount);
		dwLength += ((PWAVEHDR)lParam)->dwBytesRecorded;
		lpOriginal = (LPSTR)dataBuf;

		dwProc = convolve(hDlg, lpOriginal, lpWaveData, NULL, dwPos, dwUnit, sPitch);
		dwPos += BUFSIZE / (DWORD)wFormat.nBlockAlign;
		if (dwLength >= dwMaxSize - BUFSIZE)
			isToStop = true;
		waveInAddBuffer(hIn, (PWAVEHDR)lParam, sizeof(WAVEHDR));

		if (dwRecCnt == 4) {
			if (waveOutOpen(&hOut, WAVE_MAPPER, &wFormat,
				(DWORD)hDlg, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR) {
				MessageBox(hDlg, "WaveOutOpen error !!!", "Message Box",
					MB_ICONEXCLAMATION | MB_OK);
				return 0;
			}
			if (waveOutPrepareHeader(
				hOut, &whdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
				MessageBox(hDlg, "PrepareHeader error", "Message Box",
					MB_ICONEXCLAMATION | MB_OK);
				return 0;
			}
			if (waveOutWrite(hOut, &whdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
				MessageBox(hDlg, ch, "waveOutWrite error!", MB_ICONEXCLAMATION | MB_OK);
				return 0;
			}
		}
		return 0;

	case MM_WIM_CLOSE:
		waveInUnprepareHeader(hIn, &whdr0, sizeof(WAVEHDR));
		waveInUnprepareHeader(hIn, &whdr1, sizeof(WAVEHDR));

		str = std::to_string(dwLength / wFormat.nAvgBytesPerSec);
		str += " s recorded";
		strcpy_s(charry, sizeof(charry), str.c_str());
		Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);

		isCapturing = false;

		if (dwLength > BUFSIZE) {
			EnableMenuItem(hMenu, ID_FILE_ORIGINAL, MF_ENABLED | MF_BYCOMMAND);
			EnableMenuItem(hMenu, ID_FILE_PROCESSED, MF_ENABLED | MF_BYCOMMAND);
		}

		EnableMenuItem(hMenu, IDM_ADVANCED, MF_ENABLED | MF_BYCOMMAND);
		EnableMenuItem(hMenu, IDM_ABOUT, MF_ENABLED | MF_BYCOMMAND);
		EnableMenuItem(hMenu, IDM_EXIT, MF_ENABLED | MF_BYCOMMAND);

		Static_SetText(GetDlgItem(hDlg, IDB_REC), "RECORD");
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK), true);
		EnableWindow(GetDlgItem(hDlg, IDC_COMBO_RATE), true);
		EnableWindow(GetDlgItem(hDlg, IDC_COMBO_TAPS), true);
		if(!isAllowed)
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PITCH), true);
		return 0;

	case MM_WOM_OPEN:
		return 0;

	case MM_WOM_DONE:
		waveOutClose(hOut);
		return 0;

	case MM_WOM_CLOSE:
		waveOutUnprepareHeader(hOut, &whdr, sizeof(WAVEHDR));
		return 0;
	}
	return DefWindowProc(hDlg, msg, wParam, lParam);
}

static BOOL prepare(HWND hDlg) {
	HWND hwndCombo = GetDlgItem(hDlg, IDC_COMBO_TAPS);
	std::string str;
	char charry[80];
	int iCnt,iPer,iPrm;
	double dPer;
	BOOL isE = false;

	/* Generate FIR */
	if (isAllowed) {
		if (!prepFir(hDlg)) {
			Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Memory error!");
			isE = true;
		}
		else {
			Static_SetText(GetDlgItem(hDlg, IDC_PERCENT), "% done");
			for (iCnt = 0;iCnt <= 72;iCnt++) {
				if (iCnt > 36) {
					iPrm = 36 - iCnt;
				}
				else
					iPrm = iCnt;
				if (!genFir(hDlg, iPrm)) {
					Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Error detected!");
					iCnt = 73;
					isE = true;
				}
				dPer = (double)iCnt * 100.0 / 72.0;
				iPer = (int)dPer;
				str = std::to_string(iPer);
				strcpy_s(charry, sizeof(charry), str.c_str());
				Static_SetText(GetDlgItem(hDlg, IDC_PROGRESS), (LPSTR)charry);
			}
		}
	}
	else {
		if (!prepFir(hDlg, sPitch)) {
			Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Memory error!");
			isE = true;
		}
		else {
			if (!genFir(hDlg, sPitch)) {
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Error detected!");
				isE = true;
			}
		}
	}
	if (isE) {
		SendMessage(hwndCombo, CB_SETCURSEL, 5, (LPARAM)0);
		return(false);
	}
	return(true);

}

static BOOL GetFileName(HWND hDlg,
	BOOL bOpenName,        //// TRUE: Open, FALSE: Save
	LPSTR lpszFile,        //// File path
	int iMaxFileNmLen,     //// Maximum length of file path
	LPSTR lpszFileTitle,   //// File name
	int iMaxFileTitleLen) //// Maximum length of file name
{
	OPENFILENAME ofn;
	/* Initialize Common dialog */
	lpszFile[0] = '\0';
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hDlg;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = szOFNFilter[0];
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = lpszFile;
	ofn.nMaxFile = iMaxFileNmLen;
	ofn.lpstrFileTitle = lpszFileTitle;
	ofn.nMaxFileTitle = iMaxFileTitleLen;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = szOFNDefExt;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	/* Open Common dialogbox */
	if (bOpenName)                              //// Open
	{
		ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST |
			OFN_FILEMUSTEXIST;
		return(GetOpenFileName(&ofn));
	}
	else                                        //// Save
	{
		ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		return(GetSaveFileName(&ofn));
	}
}

static BOOL SaveWaveFile(HWND hDlg, LPSTR lpData, DWORD dwDataSize) {
	BOOL bSav;
	char szSaveFileName[MAX_RSRC_STRING_LEN];

	/* Confirm if data exist */
	if (!lpData) {
		return(FALSE);
	}
	/* Ask for File name */
	bSav = GetFileName(hDlg, FALSE,
		szSaveFileName, sizeof(szSaveFileName),
		szFileTitle, sizeof(szFileTitle));
	if (!bSav)
		return(FALSE);
	else {
		/* Save data to .WAV */
		if (!WriteWaveData(hDlg, szSaveFileName, lpData, dwDataSize, dwSrate))
			return(FALSE);
	}
	return(TRUE);
}

BOOL WriteWaveData(HWND hWnd,
	LPSTR lpszFileName,
	LPSTR lplpWaveData,
	DWORD dwdwWaveDataSize,
	DWORD dwdwSamplesPerSec)
{
	HMMIO         hmmio;
	MMCKINFO      mmckinfoWave;
	MMCKINFO      mmckinfoFmt;
	MMCKINFO      mmckinfoData;
	LONG          lFmtSize;
	LONG          lDataSize;

	hmmio = mmioOpen(lpszFileName, NULL, MMIO_ALLOCBUF | MMIO_WRITE | MMIO_CREATE);
	if (hmmio == NULL) {
		ReportError(hWnd, IDS_CANTOPENFILE);
		return(FALSE);
	}
	lFmtSize = (LONG)sizeof(pcmWaveFormat);
	/* WAVE chunk */
	mmckinfoWave.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mmckinfoWave.cksize = (LONG)dwdwWaveDataSize + lFmtSize;
	if (mmioCreateChunk(hmmio, &mmckinfoWave, MMIO_CREATERIFF) != 0) {
		ReportError(hWnd, IDS_CANTWRITEWAVE);
		mmioClose(hmmio, 0);
		return(FALSE);
	}

	/* fmt chunk */
	mmckinfoFmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mmckinfoFmt.cksize = lFmtSize;
	if (mmioCreateChunk(hmmio, &mmckinfoFmt, 0) != 0) {
		ReportError(hWnd, IDS_CANTWRITEFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* PCMWAVEFORMAT */
	pcmWaveFormat.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmWaveFormat.wf.nChannels = CHANNELS;
	pcmWaveFormat.wf.nSamplesPerSec = dwSrate;
	pcmWaveFormat.wf.nAvgBytesPerSec = dwSrate * (BITDEPTH / 8) * CHANNELS;
	pcmWaveFormat.wf.nBlockAlign = BITDEPTH * CHANNELS / 8;
	pcmWaveFormat.wBitsPerSample = BITDEPTH;
	/* Write to fmt chunk */
	if (mmioWrite(hmmio, (LPSTR)&pcmWaveFormat, lFmtSize) != lFmtSize) {
		ReportError(hWnd, IDS_CANTWRITEFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Assend */
	if (mmioAscend(hmmio, &mmckinfoFmt, 0) != 0) {
		ReportError(hWnd, IDS_CANTWRITEFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}

	/* data chunk */
	lDataSize = (LONG)dwdwWaveDataSize;
	mmckinfoData.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mmckinfoFmt.cksize = lDataSize;
	if (mmioCreateChunk(hmmio, &mmckinfoData, 0) != 0) {
		ReportError(hWnd, IDS_CANTWRITEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Write data */
	if (mmioWrite(hmmio, lplpWaveData, lDataSize) != lDataSize) {
		ReportError(hWnd, IDS_CANTWRITEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Assend */
	if (mmioAscend(hmmio, &mmckinfoData, 0) != 0) {
		ReportError(hWnd, IDS_CANTWRITEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}

	/* Assend */
	if (mmioAscend(hmmio, &mmckinfoWave, 0) != 0) {
		ReportError(hWnd, IDS_CANTWRITEWAVE);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Close file */
	mmioClose(hmmio, 0);
	return(TRUE);
}

BOOL memolocate(HWND hWnd, LPSTR *lpWavData, DWORD dwSize) {

	LPSTR lpData = (LPSTR)GlobalAllocPtr(GMEM_MOVEABLE, dwSize);
	if (!lpData) {
		ReportError(hWnd, IDS_OUTOFMEMORY);
		return(FALSE);
	}
	*lpWavData = lpData;
	return(TRUE);
}

static void FreeGlobalData(void) {
	if (lpOriginal) {
		GlobalFreePtr(lpOriginal);
		lpOriginal = NULL;
	}
	if (lpWaveData) {
		GlobalFreePtr(lpWaveData);
		lpWaveData = NULL;
	}
	if (byteBuf0) {
		free(byteBuf0);
	}
	if (byteBuf1) {
		free(byteBuf1);
	}
	if (dataBuf) {
		free(dataBuf);
	}
	return;
}

static void ReportError(HWND hWnd, int iErrorID)
{
	HINSTANCE hInstance;
	char szErrStr[MAX_RSRC_STRING_LEN];
	char szCaption[MAX_RSRC_STRING_LEN];
	hInstance = GetWindowInstance(hWnd);
	LoadString(hInstance, iErrorID, szErrStr, sizeof(szErrStr));
	LoadString(hInstance, IDS_CAPTION, szCaption, sizeof(szCaption));
	MessageBox(hWnd, szErrStr, szCaption, MB_ICONEXCLAMATION | MB_OK);
	return;
}

int WINAPI WinMain(
	HINSTANCE hinstThis,
	HINSTANCE hinstPrev,
	LPSTR lpszCmdLine,
	int iCmdShow) {

	MSG msg;
	WNDCLASS wClass;

	wClass.style = CS_HREDRAW | CS_VREDRAW;
	wClass.lpfnWndProc = manipit_WndProc;
	wClass.cbClsExtra = 0;
	wClass.cbWndExtra = DLGWINDOWEXTRA;
	wClass.hInstance = hinstThis;
	wClass.hIcon = LoadIcon(hinstThis, MAKEINTRESOURCE(IDI_ICON));
	wClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wClass.hbrBackground = (HBRUSH)CreateSolidBrush(GetSysColor(COLOR_MENU));
	wClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
	wClass.lpszClassName = szmanipitClass;

	if (!RegisterClass(&wClass)) return -1;

	DialogBox(hinstThis,
		MAKEINTRESOURCE(IDD_MAIN),
		NULL,
		(DLGPROC)manipit_WndProc);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}