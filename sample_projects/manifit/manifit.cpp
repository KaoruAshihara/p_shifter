#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>            
#include <mmsystem.h>
#include <string>
#include <vector>
#include "resource.h"
#include "p_shifter.h"

#define BUFSIZE 4096
#define MAX_RSRC_STRING_LEN 80

/*********************************
PROTOTYPE Definition
*********************************/
int WINAPI WinMain(HINSTANCE hinstThis, HINSTANCE hinstPrev, LPSTR lpszCmdLine, int iCmdShow);
static void FreeGlobalWaveData(void);
static BOOL GetFileName(HWND hDlg, BOOL bOpenName, LPSTR lpszFile,int iMaxFileNmLen, LPSTR lpszFileTitle, int iMaxFileTitleLen);
static BOOL OpenWaveFile(HWND hDlg);
BOOL ReadWaveData(HWND hWnd, LPSTR lpszFileName, LPSTR *lplpWaveData,DWORD *lpdwWaveDataSize, DWORD *lpdwSmpl);
static BOOL SaveWaveFile(HWND hDlg, DWORD dwDataSize);
BOOL WriteWaveData(HWND hWnd,LPSTR lpszFileName,LPSTR lplpWaveData,DWORD dwdwWaveDataSize,DWORD dwdwSamplesPerSec);
BOOL memolocate(HWND hWnd, LPSTR *lpWavData, DWORD dwSize);
static void ReportError(HWND hWnd, int iErrorID);

/*********************************
Global variables
*********************************/
PCMWAVEFORMAT pcmWaveFormat;
static HWAVEOUT hOut;
static WAVEHDR whdr;
static WAVEHDR whdr1;
static char szOFNDefExt[] = "WAV";								// File extension
static char *szOFNFilter[] = {"Sound Files (*.WAV)\0 *.WAV\0All Files (*.*)\0    *.*\0\0"};
static char szFileName[_MAX_PATH];									
static char szFileTitle[_MAX_FNAME];
static char szmanifitClass[] = "manifitClass";
LPSTR lpOriginal;
LPSTR lpWaveData;
static DWORD dwSPC;
DWORD dwWaveDataSize;        // size of .WAV file data 
DWORD dwNewSize;
DWORD dwSrate;
DWORD dwCurrentSample;       // position in current sound
DWORD dwPos,dwBlock;
DWORD dwUnit,dwProc;
LPSTR byteBuf[2];
unsigned channels;
unsigned BitPerSample;
static int iBufNo;
static int iLat,iThr;
static int iRelease;
static int iMark;
static short sAdv;
static short sFirNo;
short sTaps,sPitch;
static BOOL isPlaying;
static BOOL isToStop = FALSE;
static BOOL isToRew = FALSE;

/*********************************
Dialogue Procedures
*********************************/
BOOL WINAPI Adv_DlgProc(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam) {
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

BOOL WINAPI About_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		return(true);

	case WM_COMMAND:
		switch (wParam) {
		case IDOK:
			EndDialog(hDlg, TRUE);
			return (TRUE);
		}
	default:
		return(FALSE);
	}
	UNREFERENCED_PARAMETER(lParam);
}

/*********************************
Window Procedure
*********************************/
LRESULT WINAPI manifit_WndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	HWND hwndCombo = GetDlgItem(hDlg, IDC_COMBO_TAPS);
	HMENU hMenu = GetMenu(hDlg);
	RECT rc;
	int iCnt,iSel;
	int iX, iY;
	char charry[255];
	std::string str;
	std::vector<std::string> strTaps =
	{ "256","512","1024","2048","4096" };

	switch (msg) {
	case WM_INITDIALOG:
		iX = GetSystemMetrics(SM_CXSCREEN) / 2;
		iY = GetSystemMetrics(SM_CYSCREEN) / 3;
		GetClientRect(hDlg, &rc);
		iX -= rc.right / 2;
		iY -= rc.bottom / 2;
		SetWindowPos(hDlg, NULL, iX, iY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

		isPlaying = false;

		SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0L);
		for (iCnt = 0;iCnt < (int)strTaps.size();iCnt++)
			SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)strTaps[iCnt].c_str());
		SendMessage(hwndCombo, CB_SETCURSEL, 2, (LPARAM)0);
		sTaps = (short)std::stoi(strTaps[2]);

		SendMessage(GetDlgItem(hDlg, IDC_SPIN), UDM_SETRANGE, (WPARAM)0, (LPARAM)MAKELONG(36, -36));
		sPitch = 0;
		SendMessage(GetDlgItem(hDlg, IDC_SPIN), UDM_SETPOS, 0, (LPARAM)sPitch);

		iThr = -40;
		iLat = 1000;

		iRelease = (int)((float)dwSrate * (float)iLat / 1000.0F);

		return 0;

	case WM_DESTROY:
		finalize(hDlg);
		FreeGlobalWaveData();
		PostQuitMessage(0);
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam)){
		case ID_MENU_OPEN:
			if (!OpenWaveFile(hDlg))
				break;
			dwBlock = channels * (BitPerSample / 8);
			dwSPC = dwWaveDataSize / dwBlock;
			str = std::to_string(dwSPC);
			str += " samples loaded";
			strcpy_s(charry, sizeof(charry), str.c_str());
			Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);

			dwNewSize = 0;
			EnableMenuItem(hMenu, ID_FILE_SAVE, MF_GRAYED | MF_BYCOMMAND);
			EnableWindow(GetDlgItem(hDlg, IDB_SET), true);
			break;

		case ID_FILE_SAVE:
			if (!SaveWaveFile(hDlg,dwNewSize)) {
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Failed!");
			}
			else {
				strcpy_s(charry,sizeof(charry),szFileTitle);
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);
			}
			break;

		case ID_MENU_ADVANCED:
			DialogBox(GetWindowInstance(hDlg),
				MAKEINTRESOURCE(DLG_SET), hDlg, Adv_DlgProc);
			break;

		case ID_MENU_ABOUT:
			DialogBox(GetWindowInstance(hDlg),
				MAKEINTRESOURCE(IDD_ABOUT), hDlg, About_DlgProc);
			break;

		case ID_MENU_EXIT:
			FORWARD_WM_CLOSE(hDlg, PostMessage);
			break;

		case IDC_COMBO_TAPS:
			EnableWindow(GetDlgItem(hDlg, IDB_REC), false);

			break;

		case IDC_EDIT_PITCH:
			sPitch = GetDlgItemInt(hDlg, IDC_EDIT_PITCH, false, true);
			sFirNo = 0;
			firNoSetter(hDlg, sFirNo);
			break;

		case IDB_SET:
			EnableWindow(GetDlgItem(hDlg, IDB_REC), false);
			iRelease = (int)((float)dwSrate * (float)iLat / 1000.0F);

			iSel = (WORD)SendMessage(hwndCombo, CB_GETCURSEL, 0, 0L);
			sTaps = (short)std::stoi(strTaps[iSel]);

			genFunc(hDlg, sTaps);

			/* Generate FIR */
			for (iCnt = 0;iCnt < 37;iCnt++) {
				genFir(hDlg, iCnt,sTaps);
			}
			str = std::to_string(sTaps);
			str += " taps";
			strcpy_s(charry, sizeof(charry), str.c_str());
			Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);
			Static_SetText(GetDlgItem(hDlg, IDB_EXE), "EXECUTE");
			EnableWindow(GetDlgItem(hDlg, IDB_EXE), true);
			break;

		case IDB_EXE:
			if (isPlaying) {
				isToStop = true;
				waveOutReset(hOut);
			}
			else {
				EnableWindow(GetDlgItem(hDlg, IDB_EXE), false);
				EnableWindow(GetDlgItem(hDlg, IDB_SET), false);
				EnableWindow(GetDlgItem(hDlg, IDC_COMBO_TAPS), false);
				dwPos = 0;
				dwUnit = BUFSIZE / dwBlock;

				sPitch = GetDlgItemInt(hDlg, IDC_EDIT_PITCH, false, true);
				sFirNo = 0;
				sAdv = 0;
				iMark = 0;
				isToStop = false;
				isToRew = false;
				iBufNo = 0;

				initialize(hDlg, iThr, iRelease, iMark,sFirNo,sAdv,channels,isToRew);

				if (lpWaveData) {
					GlobalFreePtr(lpWaveData);
					lpWaveData = NULL;
				}
				memolocate(hDlg, &lpWaveData, dwWaveDataSize);

				byteBuf[0] = (LPSTR)malloc(BUFSIZE);
				byteBuf[1] = (LPSTR)malloc(BUFSIZE);

				dwProc = convolve(hDlg, lpOriginal, lpWaveData, byteBuf[0], dwPos, dwUnit, sPitch, sTaps);
				dwPos += dwUnit;
				dwProc = convolve(hDlg, lpOriginal, lpWaveData, byteBuf[1], dwPos, dwUnit, sPitch, sTaps);
				dwPos += dwUnit;
				dwNewSize = dwProc * dwBlock;

				whdr.lpData = byteBuf[0];
				whdr.dwBufferLength = BUFSIZE;
				whdr.dwBytesRecorded = 0;
				whdr.dwFlags = 0;
				whdr.dwLoops = 1;
				whdr.lpNext = NULL;
				whdr.dwUser = 0;
				whdr.reserved = 0;

				whdr1.lpData = byteBuf[1];
				whdr1.dwBufferLength = BUFSIZE;
				whdr1.dwBytesRecorded = 0;
				whdr1.dwFlags = 0;
				whdr1.dwLoops = 1;
				whdr1.lpNext = NULL;
				whdr1.dwUser = 0;
				whdr1.reserved = 0;

				if ((waveOutOpen((LPHWAVEOUT)&hOut, WAVE_MAPPER,
					(LPWAVEFORMATEX)&pcmWaveFormat, (DWORD)hDlg, 0L, CALLBACK_WINDOW)) != 0) {
					MessageBox(hDlg, "WaveOutOpen error !!!", "Message Box",
						MB_ICONEXCLAMATION | MB_OK);
					return(0);
				}
				if (waveOutPrepareHeader(
					hOut, &whdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
					MessageBox(hDlg, "PrepareHeader error", "Message Box",
						MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
				if (waveOutPrepareHeader(
					hOut, &whdr1, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
					MessageBox(hDlg, "PrepareHeader error", "Message Box",
						MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
				if (waveOutWrite(hOut, &whdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
					MessageBox(hDlg, "waveOutWrite error!", "Message Box", MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
				if (waveOutWrite(hOut, &whdr1, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
					MessageBox(hDlg, "waveOutWrite error!", "Message Box", MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
				Static_SetText(GetDlgItem(hDlg, IDC_MSG), "Playing");
				Static_SetText(GetDlgItem(hDlg, IDB_EXE), "STOP");
				EnableWindow(GetDlgItem(hDlg, IDB_EXE), true);
				isPlaying = true;
			}
			break;
		}
		return 0;

	case MM_WOM_OPEN:
		return 0;

	case MM_WOM_DONE:
		if (isToStop) {
			waveOutClose(hOut);
			isPlaying = false;
			Static_SetText(GetDlgItem(hDlg, IDB_EXE), "EXECUTE");
		}
		else {
			if (iBufNo == 0) {
				dwProc = convolve(hDlg, lpOriginal, lpWaveData, byteBuf[0], dwPos, dwUnit, sPitch, sTaps);
				dwPos += dwUnit;
				dwNewSize = dwProc * dwBlock;
				iBufNo++;
				if (waveOutWrite(hOut, &whdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
					MessageBox(hDlg, "waveOutWrite error!", "Message Box", MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
			}
			else {
				dwProc = convolve(hDlg, lpOriginal, lpWaveData, byteBuf[1], dwPos, dwUnit, sPitch, sTaps);
				dwPos += dwUnit;
				dwNewSize = dwProc * dwBlock;
				iBufNo = 0;
				if (waveOutWrite(hOut, &whdr1, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
					MessageBox(hDlg, "waveOutWrite error!", "Message Box", MB_ICONEXCLAMATION | MB_OK);
					return 0;
				}
			}
			if (dwNewSize >= dwWaveDataSize - BUFSIZE)
				isToStop = true;
		}
		return 0;

	case MM_WOM_CLOSE:
		waveOutUnprepareHeader(hOut, &whdr, sizeof(WAVEHDR));
		EnableWindow(GetDlgItem(hDlg, IDB_SET), true);
		EnableWindow(GetDlgItem(hDlg, IDC_COMBO_TAPS), true);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PITCH), true);
		EnableWindow(GetDlgItem(hDlg, IDC_SPIN), true);
		if (dwNewSize > 0) {
			str = std::to_string(dwNewSize);
			str += " bytes processed";
			strcpy_s(charry, sizeof(charry), str.c_str());
			Static_SetText(GetDlgItem(hDlg, IDC_MSG), (LPSTR)charry);
			EnableMenuItem(hMenu, ID_FILE_SAVE, MF_ENABLED | MF_BYCOMMAND);
		}
		return 0;
	}
	return DefWindowProc(hDlg, msg, wParam, lParam);
}

/*********************************
Functions
*********************************/
static void FreeGlobalWaveData(void) {
	if (lpOriginal){
		GlobalFreePtr(lpOriginal);
		lpOriginal = NULL;
	}

	if (lpWaveData){
		GlobalFreePtr(lpWaveData);
		lpWaveData = NULL;
	}

	free(byteBuf[0]);
	free(byteBuf[1]);
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

static BOOL OpenWaveFile(HWND hDlg)
{
	/* Open WAV file dialogbox */
	if (!GetFileName(hDlg, TRUE, szFileName, sizeof(szFileName),
		szFileTitle, sizeof(szFileTitle)))
		return(FALSE);
	FreeGlobalWaveData();               //// Free memory
										 /* Load data */
	if (!ReadWaveData(hDlg, szFileName, &lpOriginal, &dwWaveDataSize, &dwSrate))
		return(FALSE);
	dwCurrentSample = 0;
	return(TRUE);
}

BOOL ReadWaveData(HWND hWnd,
	LPSTR lpszFileName,
	LPSTR *lplpWaveData,         //// Pointer of WAVE data
	DWORD *lpdwWaveDataSize,     //// Size of data
	DWORD *lpdwSamplesPerSec)    //// Sample rate
{
	HMMIO         hmmio;            //// File handle
	MMCKINFO      mmckinfoWave;     //// 'WAVE' chunk info
	MMCKINFO      mmckinfoFmt;      //// 'fmt ' chunk info
	MMCKINFO      mmckinfoData;     //// 'data' chunk info
	LONG          lFmtSize;         //// 'fmt ' chunk size
	LPSTR         lpData;           //// Data
	LONG		  lDataSize;

	hmmio = mmioOpen(lpszFileName, NULL, MMIO_ALLOCBUF | MMIO_READ);

	if (hmmio == NULL)
	{
		ReportError(hWnd, IDS_CANTOPENFILE);
		return(FALSE);
	}
	/* WAVE chunk */
	mmckinfoWave.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if (mmioDescend(hmmio, &mmckinfoWave, NULL, MMIO_FINDRIFF) != 0)
	{
		ReportError(hWnd, IDS_NOTWAVEFILE);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* fmt chunk */
	mmckinfoFmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(hmmio, &mmckinfoFmt, &mmckinfoWave,
		MMIO_FINDCHUNK) != 0)
	{
		ReportError(hWnd, IDS_CORRUPTEDFILE);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Read format info */
	lFmtSize = (LONG)sizeof(pcmWaveFormat);
	if (mmioRead(hmmio, (LPSTR)&pcmWaveFormat, lFmtSize) != lFmtSize)
	{
		ReportError(hWnd, IDS_CANTREADFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	if (mmioAscend(hmmio, &mmckinfoFmt, 0) != 0)
	{
		ReportError(hWnd, IDS_CANTREADFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Check if the format is suported */
	if ((pcmWaveFormat.wf.wFormatTag != WAVE_FORMAT_PCM)
		|| (pcmWaveFormat.wf.nSamplesPerSec > 96000)
		|| (pcmWaveFormat.wf.nSamplesPerSec < 500)){
		ReportError(hWnd, IDS_UNSUPPORTEDFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	else if (pcmWaveFormat.wBitsPerSample != 16) {
		ReportError(hWnd, IDS_UNSUPPORTEDFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	else if (pcmWaveFormat.wf.nChannels != 1 && pcmWaveFormat.wf.nChannels != 2)
	{
		ReportError(hWnd, IDS_UNSUPPORTEDFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* DATA chunk */
	mmckinfoData.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(hmmio, &mmckinfoData, &mmckinfoWave, MMIO_FINDCHUNK) != 0)
	{
		ReportError(hWnd, IDS_CORRUPTEDFILE);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	lDataSize = (LONG)mmckinfoData.cksize;	// Data size
	if (lDataSize == 0)
	{
		ReportError(hWnd, IDS_NOWAVEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	lpData = (LPSTR)GlobalAllocPtr(GMEM_MOVEABLE, lDataSize);
	if (!lpData)
	{
		ReportError(hWnd, IDS_OUTOFMEMORY);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Load data */
	if (mmioRead(hmmio, (LPSTR)lpData, lDataSize) != lDataSize)
	{
		ReportError(hWnd, IDS_CANTREADDATA);
		GlobalFreePtr(lpData);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Close file */
	mmioClose(hmmio, 0);

	*lplpWaveData = lpData;
	*lpdwWaveDataSize = (DWORD)lDataSize;
	*lpdwSamplesPerSec = pcmWaveFormat.wf.nSamplesPerSec;
	channels = pcmWaveFormat.wf.nChannels;
	BitPerSample = pcmWaveFormat.wBitsPerSample;

	return(TRUE);

}

static BOOL SaveWaveFile(HWND hDlg,DWORD dwDataSize){
	BOOL bSav;
	char szSaveFileName[MAX_RSRC_STRING_LEN];

	/* Confirm if data exist */
	if (!lpWaveData) {
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
		if (!WriteWaveData(hDlg, szSaveFileName, lpWaveData, dwDataSize, dwSrate))
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
	if (hmmio == NULL){
		ReportError(hWnd, IDS_CANTOPENFILE);
		return(FALSE);
	}
	lFmtSize = (LONG)sizeof(pcmWaveFormat);
	/* WAVE chunk */
	mmckinfoWave.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mmckinfoWave.cksize = (LONG)dwdwWaveDataSize + lFmtSize;
	if (mmioCreateChunk(hmmio, &mmckinfoWave, MMIO_CREATERIFF) != 0){
		ReportError(hWnd, IDS_CANTWRITEWAVE);
		mmioClose(hmmio, 0);
		return(FALSE);
	}

	/* fmt chunk */
	mmckinfoFmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mmckinfoFmt.cksize = lFmtSize;
	if (mmioCreateChunk(hmmio, &mmckinfoFmt, 0) != 0){
		ReportError(hWnd, IDS_CANTWRITEFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* PCMWAVEFORMAT */
	pcmWaveFormat.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmWaveFormat.wf.nChannels = channels;
	pcmWaveFormat.wf.nSamplesPerSec = dwSrate;
	pcmWaveFormat.wf.nAvgBytesPerSec = dwSrate * (BitPerSample / 8) * channels;
	pcmWaveFormat.wf.nBlockAlign = BitPerSample * channels / 8;
	pcmWaveFormat.wBitsPerSample = BitPerSample;
	/* Write to fmt chunk */
	if (mmioWrite(hmmio, (LPSTR)&pcmWaveFormat, lFmtSize) != lFmtSize){
		ReportError(hWnd, IDS_CANTWRITEFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Assend */
	if (mmioAscend(hmmio, &mmckinfoFmt, 0) != 0){
		ReportError(hWnd, IDS_CANTWRITEFORMAT);
		mmioClose(hmmio, 0);
		return(FALSE);
	}

	/* data chunk */
	lDataSize = (LONG)dwdwWaveDataSize;
	mmckinfoData.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mmckinfoFmt.cksize = lDataSize;
	if (mmioCreateChunk(hmmio, &mmckinfoData, 0) != 0){
		ReportError(hWnd, IDS_CANTWRITEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Write data */
	if (mmioWrite(hmmio, lplpWaveData, lDataSize) != lDataSize){
		ReportError(hWnd, IDS_CANTWRITEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}
	/* Assend */
	if (mmioAscend(hmmio, &mmckinfoData, 0) != 0){
		ReportError(hWnd, IDS_CANTWRITEDATA);
		mmioClose(hmmio, 0);
		return(FALSE);
	}

	/* Assend */
	if (mmioAscend(hmmio, &mmckinfoWave, 0) != 0){
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
	wClass.lpfnWndProc = manifit_WndProc;
	wClass.cbClsExtra = 0;
	wClass.cbWndExtra = DLGWINDOWEXTRA;
	wClass.hInstance = hinstThis;
	wClass.hIcon = LoadIcon(hinstThis, MAKEINTRESOURCE(ICON_ASHI));
	wClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wClass.hbrBackground = (HBRUSH)CreateSolidBrush(GetSysColor(COLOR_MENU));
	wClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
	wClass.lpszClassName = szmanifitClass;

	if (!RegisterClass(&wClass)) return -1;

	DialogBox(hinstThis,
		MAKEINTRESOURCE(IDD_MAIN),
		NULL,
		(DLGPROC)manifit_WndProc);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}