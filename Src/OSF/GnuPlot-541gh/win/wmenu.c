// GNUPLOT - win/wmenu.c 
// Copyright 1992, 1993, 1998, 2004   Maurice Castro, Russell Lang
//
#include <gnuplot.h>
#pragma hdrstop
#define STRICT
#define COBJMACROS
//#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <winuser.h>
#include <shlwapi.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __WATCOMC__
	#include <direct.h>
#endif
#include "wgnuplib.h"
#include "wresourc.h"
#include "wcommon.h"
#include "winmain.h"
#include "wgdiplus.h"

#define MBOXTITLE lptw->Title // title of error messages 
// limits 
#define MAXSTR 255
#define MACROLEN 10000
// #define NUMMENU 256  defined in wresourc.h 
#define MENUDEPTH 3

/* menu tokens */
#define CMDMIN 129
#define INPUT 129
#define EOS 130
#define OPEN 131
#define SAVE 132
#define DIRECTORY 133
#define OPTIONS 134
#define ABOUT 135
#define CMDMAX 135
static const char * keyword[] = {
	"[INPUT]", "[EOS]", "[OPEN]", "[SAVE]", "[DIRECTORY]",
	"[OPTIONS]", "[ABOUT]",
	"{ENTER}", "{ESC}", "{TAB}",
	"{^A}", "{^B}", "{^C}", "{^D}", "{^E}", "{^F}", "{^G}", "{^H}",
	"{^I}", "{^J}", "{^K}", "{^L}", "{^M}", "{^N}", "{^O}", "{^P}",
	"{^Q}", "{^R}", "{^S}", "{^T}", "{^U}", "{^V}", "{^W}", "{^X}",
	"{^Y}", "{^Z}", "{^[}", "{^\\}", "{^]}", "{^^}", "{^_}",
	NULL
};
static const BYTE keyeq[] = {
	INPUT, EOS, OPEN, SAVE, DIRECTORY,
	OPTIONS, ABOUT,
	13, 27, 9,
	1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 28, 29, 30, 31,
	0
};

#define GBUFSIZE 512
typedef struct tagGFILE {
	FILE * hfile;
	char   getbuf[GBUFSIZE];
	int    getnext;
	int    getleft;
} GFILE;

static GFILE * Gfopen(LPCTSTR FileName, LPCTSTR Mode);
static int Gfgets(LPSTR lp, int size, GFILE * gfile);
static int GetLine(char * buffer, int len, GFILE * gfile);
static void LeftJustify(char * d, char * s);
static BYTE MacroCommand(TW * lptw, UINT m);
static void TranslateMacro(char * string);
static INT_PTR CALLBACK InputBoxDlgProc(HWND, UINT, WPARAM, LPARAM);
// 
// Note: this code has been bluntly copied from MSDN article KB179378
//   "How To Browse for Folders from the Current Directory"
// 
static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	TCHAR szDir[MAX_PATH];
	switch(uMsg) {
		case BFFM_INITIALIZED:
		    if(GetCurrentDirectory(sizeof(szDir) / sizeof(TCHAR), szDir)) {
			    // WParam is TRUE since you are passing a path. It would be FALSE if you were passing a pidl. 
			    SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)szDir);
		    }
		    break;
		case BFFM_SELCHANGED:
		    // Set the status window to the currently selected path. 
		    if(SHGetPathFromIDList((LPITEMIDLIST)lp, szDir)) {
			    SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM)szDir);
		    }
		    break;
	}
	return 0;
}

static BYTE MacroCommand(TW * lptw, UINT m)
{
	BYTE * s = lptw->P_LpMw->macro[m];
	while(s && *s) {
		if((*s >= CMDMIN) && (*s <= CMDMAX))
			return *s;
		s++;
	}
	return 0;
}
//
// Send a macro to the text window 
//
void SendMacro(TW * lptw, UINT m)
{
	MW * lpmw = lptw->P_LpMw;
	BOOL flag = TRUE;
	int i;
	LPWSTR buf = (LPWSTR)LocalAllocPtr(LHND, (MAXSTR + 1) * sizeof(WCHAR));
	if(buf && static_cast<int>(m) < lpmw->nCountMenu) {
		BYTE * s = lpmw->macro[m];
		LPWSTR d = buf;
		*d = '\0';
		while(s && *s && (d - buf < MAXSTR)) {
			if(*s >= CMDMIN && *s <= CMDMAX) {
				switch(*s) {
					case SAVE: /* [SAVE] - get a save filename from a file list box */
					case OPEN: { /* [OPEN] - get a filename from a file list box */
						OPENFILENAMEW ofn;
						LPWSTR szFile;
						LPWSTR szFilter;
						LPWSTR szTitle;
						BOOL save;
						WCHAR cwd[MAX_PATH];
						char str[MAXSTR + 1];
						if((szTitle = (LPWSTR)LocalAllocPtr(LHND, (MAXSTR + 1) * sizeof(WCHAR))) == NULL)
							return;
						if((szFile = (LPWSTR)LocalAllocPtr(LHND, (MAXSTR + 1) * sizeof(WCHAR))) == NULL)
							return;
						if((szFilter = (LPWSTR)LocalAllocPtr(LHND, (MAXSTR + 1) * sizeof(WCHAR))) == NULL)
							return;
						// FIXME: This is still not safe wrt. buffer overflows. 
						save = (*s == SAVE);
						s++;
						for(i = 0; (*s >= 32) && (*s <= 126); i++)
							str[i] = *s++; /* get dialog box title */
						str[i] = '\0';
						MultiByteToWideChar(CP_ACP, 0, str, MAXSTR + 1, szTitle, MAXSTR + 1);
						s++;
						for(i = 0; (*s >= 32) && (*s <= 126); i++)
							str[i] = *s++; /* temporary copy of filter */
						str[i++] = '\0';
						MultiByteToWideChar(CP_ACP, 0, str, MAXSTR + 1, szFile, MAXSTR + 1);
						sstrcpy(szFilter, L"Default (");
						wcscat(szFilter, szFile);
						wcscat(szFilter, L")");
						i = wcslen(szFilter);
						i++; /* move past NUL */
						sstrcpy(szFilter + i, szFile);
						i += wcslen(szFilter + i);
						i++; /* move past NUL */
						sstrcpy(szFilter + i, L"All Files (*.*)");
						i += wcslen(szFilter + i);
						i++; /* move past NUL */
						sstrcpy(szFilter + i, L"*.*");
						i += wcslen(szFilter + i);
						i++; /* move past NUL */
						szFilter[i++] = '\0'; /* add a second NUL */
						flag = 0;
						szFile[0] = '\0';
						/* clear the structure */
						memzero(&ofn, sizeof(ofn));
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = lptw->hWndParent;
						ofn.lpstrFilter = szFilter;
						ofn.nFilterIndex = 1;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = MAXSTR;
						ofn.lpstrFileTitle = szFile;
						ofn.nMaxFileTitle = MAXSTR;
						ofn.lpstrTitle = szTitle;
						/* Windows has it's very special meaning of 'default directory'  */
						/* (search for OPENFILENAME on MSDN). So we set it explicitly: */
						/* ofn.lpstrInitialDir = NULL; */
						_wgetcwd(cwd, sizeof(cwd) / sizeof(WCHAR));
						ofn.lpstrInitialDir = cwd;
						ofn.Flags = OFN_SHOWHELP | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
						flag = (save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn));
						if(flag) {
							lpmw->nChar = wcslen(ofn.lpstrFile);
							for(i = 0; i < lpmw->nChar; i++)
								*d++ = ofn.lpstrFile[i];
						}
						LocalFreePtr(szTitle);
						LocalFreePtr(szFilter);
						LocalFreePtr(szFile);
						break;
					}
					case INPUT: /* [INPUT] - input a string of characters */
						s++;
						for(i = 0; (*s >= 32 && *s <= 126); i++)
							lpmw->szPrompt[i] = *s++;
						lpmw->szPrompt[i] = '\0';
						flag = DialogBox(hdllInstance, TEXT("InputDlgBox"), lptw->hWndParent, InputBoxDlgProc);
						if(flag) {
							for(i = 0; i < lpmw->nChar; i++)
								*d++ = lpmw->szAnswer[i];
						}
						break;
					case DIRECTORY: { /* [DIRECTORY] - show standard directory dialog */
						BROWSEINFOW bi;
						LPITEMIDLIST pidl;
						LPWSTR szTitle;
						char str [MAXSTR + 1];
						/* allocate some space */
						if((szTitle = (LPWSTR)LocalAllocPtr(LHND, (MAXSTR + 1) * sizeof(WCHAR))) == NULL)
							return;

						/* get dialog box title */
						s++;
						for(i = 0; (*s >= 32 && *s <= 126); i++)
							str[i] = *s++;
						str[i] = '\0';
						MultiByteToWideChar(CP_ACP, 0, str, MAXSTR + 1, szTitle, MAXSTR + 1);
						flag = 0;
						/* Use the Shell's internal directory chooser. */
						/* Note: This code does not work NT 3.51 and Win32s.
									Windows 95 has shell32.dll version 4.0, but does not
									have a version number, so this will return FALSE.
						 */
						// Make sure that the installed shell version supports this approach 
						//if(GetDllVersion(TEXT("shell32.dll")) >= PACKVERSION(4, 0)) {
						if(SDynLibrary::GetVersion("shell32.dll").IsGe(4, 0, 0)) {
							MEMSZERO(bi);
							bi.hwndOwner = lptw->hWndParent;
							bi.pidlRoot = NULL;
							bi.pszDisplayName = NULL;
							bi.lpszTitle = szTitle;
							// BIF_NEWDIALOGSTYLE is supported by Win 2000 or later (Version 5.0) 
							bi.ulFlags = BIF_NEWDIALOGSTYLE|BIF_EDITBOX|BIF_STATUSTEXT|BIF_RETURNONLYFSDIRS|BIF_RETURNFSANCESTORS;
							bi.lpfn = BrowseCallbackProc;
							bi.lParam = 0;
							bi.iImage = 0;
							pidl = SHBrowseForFolderW(&bi);
							if(pidl) {
								LPMALLOC pMalloc;
								WCHAR szPath[MAX_PATH];
								uint len;
								// Convert the item ID list's binary representation into a file system path 
								SHGetPathFromIDListW(pidl, szPath);
								len = wcslen(szPath);
								flag = len > 0;
								if(flag)
									for(i = 0; i < static_cast<int>(len); i++)
										*d++ = szPath[i];
								// Allocate a pointer to an IMalloc interface. Get the address of our task allocator's IMalloc interface. 
								SHGetMalloc(&pMalloc);
								pMalloc->Free(pidl); // Free the item ID list allocated by SHGetSpecialFolderLocation 
								pMalloc->Release(); // Free our task allocator 
							}
						}
						else {
							sstrcpy(lpmw->szPrompt, szTitle);
							flag = DialogBox(hdllInstance, TEXT("InputDlgBox"), lptw->hWndParent, InputBoxDlgProc);
							if(flag) {
								for(i = 0; i < lpmw->nChar; i++)
									*d++ = lpmw->szAnswer[i];
							}
						}
						LocalFreePtr(szTitle);
						break;
					}

					case OPTIONS: { /* [OPTIONS] - open popup menu */
						POINT pt;
						RECT rect;
						int index;
						s++;
						/* align popup with toolbar button */
						index = lpmw->nButton - (lpmw->nCountMenu - m);
						if(SendMessage(lpmw->hToolbar, TB_GETITEMRECT, (WPARAM)index, (LPARAM)&rect)) {
							pt.x = rect.left;
							pt.y = rect.bottom + 1;
							ClientToScreen(lptw->hWndParent, &pt);
						}
						else {
							GetCursorPos(&pt);
						}
						TrackPopupMenu(lptw->hPopMenu, TPM_LEFTALIGN | TPM_TOPALIGN,
						pt.x, pt.y, 0, lptw->hWndParent, NULL);
						break;
					}
					case ABOUT: /* [ABOUT] - display About box */
						s++;
						SendMessage(lptw->hWndText, WM_COMMAND, M_ABOUT, (LPARAM)0);
						break;
					case EOS: /* [EOS] - End Of String - do nothing */
					default:
						s++;
						break;
				} /* switch */
				if(!flag) { /* abort */
					d = buf;
					s = (BYTE*)"";
				}
			}
			else {
				*d++ = *s++;
			}
		}
		*d = '\0';
		if(buf[0]) {
			d = buf;
			while(*d) {
				SendMessageW(lptw->hWndText, WM_CHAR, *d, 1L);
				d++;
			}
		}
	}
}

static GFILE * Gfopen(LPCTSTR FileName, LPCTSTR Mode)
{
	GFILE * gfile = (GFILE*)SAlloc::M(sizeof(GFILE));
	if(gfile) {
		gfile->hfile = _tfopen(FileName, Mode);
		if(gfile->hfile == NULL) {
			SAlloc::F(gfile);
			return NULL;
		}
		gfile->getleft = 0;
		gfile->getnext = 0;
	}
	return gfile;
}

static void Gfclose(GFILE * gfile)
{
	if(gfile) {
		if(gfile->hfile)
			fclose(gfile->hfile);
		SAlloc::F(gfile);
	}
}

/* returns number of characters read */
static int Gfgets(LPSTR lp, int size, GFILE * gfile)
{
	int i;
	int ch;
	for(i = 0; i < size; i++) {
		if(gfile->getleft <= 0) {
			if((gfile->getleft = fread(gfile->getbuf, 1, GBUFSIZE, gfile->hfile)) == 0)
				break;
			gfile->getnext = 0;
		}
		ch = *lp++ = gfile->getbuf[gfile->getnext++];
		gfile->getleft--;
		if(ch == '\r') {
			i--;
			lp--;
		}
		if(ch == '\n') {
			i++;
			break;
		}
	}
	if(i < size)
		*lp++ = '\0';
	return i;
}
//
// Get a line from the menu file 
// Return number of lines read from file including comment lines 
//
static int GetLine(char * buffer, int len, GFILE * gfile)
{
	int nLine = 0;
	BOOL status = (Gfgets(buffer, len, gfile) != 0);
	nLine++;
	while(status && (buffer[0] == 0 || buffer[0] == '\n' || buffer[0] == ';')) {
		// blank line or comment - ignore 
		status = (Gfgets(buffer, len, gfile) != 0);
		nLine++;
	}
	if(!isempty(buffer))
		buffer[strlen(buffer)-1] = '\0'; // remove trailing \n
	if(!status)
		nLine = 0; // zero lines if file error
	return nLine;
}
//
// Left justify string 
//
static void LeftJustify(char * d, char * s)
{
	while(*s && (*s == ' ' || *s == '\t'))
		s++; /* skip over space */
	do {
		*d++ = *s;
	} while(*s++);
}
//
// Translate string to tokenized macro 
//
static void TranslateMacro(char * string)
{
	for(int i = 0; keyword[i]; i++) {
		LPSTR ptr = strstr(string, keyword[i]);
		if(ptr) {
			size_t len = strlen(keyword[i]);
			*ptr = keyeq[i];
			strcpy(ptr + 1, ptr + len);
			i--; // allows for more than one occurrence of keyword 
		}
	}
}

/* Load Macros, and create Menu from Menu file */
void LoadMacros(TW * lptw)
{
	GFILE * menufile;
	BYTE * macroptr;
	char * buf;
	LPWSTR wbuf;
	int nMenuLevel;
	HMENU hMenu[MENUDEPTH + 1];
	int nLine = 1;
	int nInc;
	HGLOBAL hmacro, hmacrobuf;
	int i;
	RECT rect;
	char * ButtonText[BUTTONMAX];
	int ButtonIcon[BUTTONMAX];
	char * ButtonIconFile[BUTTONMAX];
	const int ButtonExtra = 15 + 5 + 12; /* number of standard icons */
	int ButtonIndex = 0;
	int bLoadStandardButtons = FALSE;
	int ButtonSize = 16;
	UINT dpi = GetDPI();
	TBADDBITMAP bitmap = {0};
	MW * lpmw = lptw->P_LpMw;
	// mark all buffers and menu file as unused 
	buf = NULL;
	hmacro = 0;
	hmacrobuf = 0;
	lpmw->macro = NULL;
	lpmw->macrobuf = NULL;
	lpmw->szPrompt = NULL;
	lpmw->szAnswer = NULL;
	menufile = NULL;
	// open menu file 
	if((menufile = Gfopen(lpmw->szMenuName, TEXT("rb"))) == NULL)
		goto errorcleanup;
	// allocate buffers 
	if((buf = (char *)LocalAllocPtr(LHND, MAXSTR)) == NULL)
		goto nomemory;
	if((wbuf = (LPWSTR)LocalAllocPtr(LHND, MAXSTR * sizeof(WCHAR))) == NULL)
		goto nomemory;
	hmacro = GlobalAlloc(GHND, (NUMMENU)*sizeof(BYTE *));
	if((lpmw->macro = (BYTE**)GlobalLock(hmacro))  == NULL)
		goto nomemory;
	hmacrobuf = GlobalAlloc(GHND, MACROLEN);
	if((lpmw->macrobuf = (BYTE*)GlobalLock(hmacrobuf)) == NULL)
		goto nomemory;
	if((lpmw->szPrompt = (LPWSTR)LocalAllocPtr(LHND, MAXSTR * sizeof(WCHAR))) == NULL)
		goto nomemory;
	if((lpmw->szAnswer = (LPWSTR)LocalAllocPtr(LHND, MAXSTR * sizeof(WCHAR))) == NULL)
		goto nomemory;
	macroptr = lpmw->macrobuf;
	lpmw->nButton = 0;
	lpmw->nCountMenu = 0;
	lpmw->hMenu = hMenu[0] = CreateMenu();
	nMenuLevel = 0;
	while((nInc = GetLine(buf, MAXSTR, menufile)) != 0) {
		nLine += nInc;
		LeftJustify(buf, buf);
		if(!buf[0]) {
			// ignore blank lines 
		}
		else if(sstreqi_ascii(buf, "[Menu]")) {
			// new menu 
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				swprintf_s(wbuf, MAXSTR, L"Problem on line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			if(nMenuLevel < MENUDEPTH) {
				nMenuLevel++;
			}
			else {
				swprintf_s(wbuf, MAXSTR, L"Menu is too deep at line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			hMenu[nMenuLevel] = CreateMenu();
			MultiByteToWideChar(CP_ACP, 0, buf, MAXSTR, wbuf, MAXSTR);
			AppendMenuW(hMenu[nMenuLevel > 0 ? nMenuLevel - 1 : 0], MF_STRING | MF_POPUP, (UINT_PTR)hMenu[nMenuLevel], wbuf);
		}
		else if(sstreqi_ascii(buf, "[EndMenu]")) {
			if(nMenuLevel > 0)
				nMenuLevel--; // back up one menu 
		}
		else if(sstreqi_ascii(buf, "[ButtonSize]")) {
			uint size;
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				swprintf_s(wbuf, MAXSTR, L"Problem on line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			errno = 0;
			size = strtoul(buf, NULL, 10);
			if(errno == 0 && size > 0) {
				ButtonSize = size;
			}
			else {
				swprintf_s(wbuf, MAXSTR, L"Invalid button size on line %d\n", nLine);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
		}
		else if(sstreqi_ascii(buf, "[Button]")) {
			// button macro 
			if(lpmw->nButton >= BUTTONMAX) {
				swprintf_s(wbuf, MAXSTR, L"Too many buttons at line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				swprintf_s(wbuf, MAXSTR, L"Problem on line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			if(static_cast<ssize_t>(strlen(buf) + 1) < (MACROLEN - (macroptr - lpmw->macrobuf))) {
				strcpy((char *)macroptr, buf);
			}
			else {
				swprintf_s(wbuf, MAXSTR, L"Out of space for storing menu macros\n at line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			ButtonText[lpmw->nButton] = (char *)macroptr;
			ButtonIcon[lpmw->nButton] = I_IMAGENONE; /* comctl 5.81, Win 2000 */
			{
				char * icon = strchr((char *)macroptr, ';');
				if(icon) {
					int inumber;
					*icon = '\0';
					errno = 0;
					inumber = strtoul(++icon, NULL, 10);
					if(errno == 0 && inumber != 0) {
						ButtonIcon[lpmw->nButton] = inumber;
						ButtonIconFile[lpmw->nButton] = NULL;
						bLoadStandardButtons = TRUE;
					}
					else {
						/* strip trailing white space */
						char * end = icon + strlen(icon) - 1;
						while(isspace((uchar)*end)) {
							*end = '\0';
							end--;
						}
						ButtonIcon[lpmw->nButton] = ButtonIndex;
						ButtonIconFile[lpmw->nButton] = sstrdup(icon);
						ButtonIndex++;
					}
				}
			}
			macroptr += strlen((char *)macroptr)  + 1;
			*macroptr = '\0';
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				swprintf_s(wbuf, MAXSTR, L"Problem on line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			TranslateMacro(buf);
			if(static_cast<ssize_t>(strlen(buf) + 1) < (MACROLEN - (macroptr - lpmw->macrobuf)))
				strcpy((char *)macroptr, buf);
			else {
				swprintf_s(wbuf, MAXSTR, L"Out of space for storing menu macros\n at line %d of " TCHARFMT " \n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			lpmw->hButtonID[lpmw->nButton] = lpmw->nCountMenu;
			lpmw->macro[lpmw->nCountMenu] = macroptr;
			macroptr += strlen((char *)macroptr) + 1;
			*macroptr = '\0';
			lpmw->nCountMenu++;
			lpmw->nButton++;
		}
		else {
			/* menu item */
			if(lpmw->nCountMenu >= NUMMENU) {
				swprintf_s(wbuf, MAXSTR, L"Too many menu items at line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
				lptw->PopupError(wbuf);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			/* HBB 981202: added MF_SEPARATOR to the MF_MENU*BREAK items. This is meant
			 * to maybe avoid a CodeGuard warning about passing last argument zero
			 * when item style is not SEPARATOR... Actually, a better solution would
			 * have been to combine the '|' divider with the next menu item. */
			if(buf[0] == '-') {
				if(nMenuLevel == 0)
					AppendMenu(hMenu[0], MF_SEPARATOR | MF_MENUBREAK, 0, NULL);
				else
					AppendMenu(hMenu[nMenuLevel], MF_SEPARATOR, 0, NULL);
			}
			else if(buf[0] == '|') {
				AppendMenu(hMenu[nMenuLevel], MF_SEPARATOR | MF_MENUBARBREAK, 0, NULL);
			}
			else {
				MultiByteToWideChar(CP_ACP, 0, buf, MAXSTR, wbuf, MAXSTR);
				AppendMenuW(hMenu[nMenuLevel], MF_STRING, lpmw->nCountMenu, wbuf);
				if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
					nLine += nInc;
					swprintf_s(wbuf, MAXSTR, L"Problem on line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
					lptw->PopupError(wbuf);
					goto errorcleanup;
				}
				LeftJustify(buf, buf);
				TranslateMacro(buf);
				if(static_cast<ssize_t>(strlen(buf) + 1) < (MACROLEN - (macroptr - lpmw->macrobuf)))
					strcpy((char *)macroptr, buf);
				else {
					swprintf_s(wbuf, MAXSTR, L"Out of space for storing menu macros\n at line %d of " TCHARFMT "\n", nLine, lpmw->szMenuName);
					lptw->PopupError(wbuf);
					goto errorcleanup;
				}
				lpmw->macro[lpmw->nCountMenu] = macroptr;
				macroptr += strlen((char *)macroptr) + 1;
				*macroptr = '\0';
				lpmw->nCountMenu++;
			}
		}
	}
	if((lpmw->nCountMenu - lpmw->nButton) > 0) {
		/* we have a menu bar so put it on the window */
		SetMenu(lptw->hWndParent, lpmw->hMenu);
		DrawMenuBar(lptw->hWndParent);
	}
	if(!lpmw->nButton)
		goto cleanup; /* no buttons */
	// create a toolbar 
	lpmw->hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | TBSTYLE_LIST | TBSTYLE_TOOLTIPS, 0, 0, 0, 0, lptw->hWndToolbar, (HMENU)ID_TOOLBAR, lptw->hInstance, NULL);
	if(lpmw->hToolbar == NULL)
		goto cleanup;
	SendMessage(lpmw->hToolbar, TB_SETEXTENDEDSTYLE, 0, (LPARAM)TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_HIDECLIPPEDBUTTONS);
	SendMessage(lpmw->hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	bitmap.hInst = HINST_COMMCTRL;
	// set size of toolbar icons 
	// loading standard bitmaps forces a specific button size 
	if(bLoadStandardButtons)
		ButtonSize = (dpi <= 96) ? 16 : 24;
	SendMessage(lpmw->hToolbar, TB_SETBITMAPSIZE, (WPARAM)0, MAKELPARAM(ButtonSize, ButtonSize));
	// only load standard bitmaps if required. 
	if(bLoadStandardButtons) {
		bitmap.nID = (dpi > 96)  ? IDB_STD_LARGE_COLOR : IDB_STD_SMALL_COLOR;
		SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&bitmap));
		bitmap.nID = (dpi > 96)  ? IDB_HIST_LARGE_COLOR : IDB_HIST_SMALL_COLOR;
		SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&bitmap));
		bitmap.nID = (dpi > 96)  ? IDB_VIEW_LARGE_COLOR : IDB_VIEW_SMALL_COLOR;
		SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&bitmap));
	}
	// create buttons 
	for(i = 0; i < lpmw->nButton; i++) {
		TBBUTTON button;
		memzero(&button, sizeof(button));
		button.iBitmap = ButtonIcon[i];
		if(ButtonIconFile[i]) {
#ifdef HAVE_GDIPLUS
			char * fname;
			LPWSTR wfname;
#ifdef GNUPLOT_SHARE_DIR
			fname = RelativePathToGnuplot(GNUPLOT_SHARE_DIR "\\images");
#else
			fname = RelativePathToGnuplot("images");
#endif
			fname = (char *)SAlloc::R(fname, strlen(fname) + strlen(ButtonIconFile[i]) + 2);
			PATH_CONCAT(fname, ButtonIconFile[i]);
			if(bLoadStandardButtons)
				button.iBitmap += ButtonExtra;
			wfname = UnicodeText(fname, S_ENC_DEFAULT);
			bitmap.hInst = NULL;
			bitmap.nID = (UINT_PTR)gdiplusLoadBitmap(wfname, ButtonSize);
			SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, (LPARAM)&bitmap);
			SAlloc::F(fname);
			SAlloc::F(wfname);
#else
			static bool warn_once = TRUE;
			if(warn_once)
				fprintf(stderr, "Error:  This version of gnuplot cannot load icon bitmaps as it does not include support for GDI+.\n");
			warn_once = FALSE;
#endif
		}
		button.idCommand = lpmw->hButtonID[i];
		button.fsState = TBSTATE_ENABLED;
		button.fsStyle = BTNS_AUTOSIZE | BTNS_NOPREFIX;
		if(MacroCommand(lptw, lpmw->hButtonID[i]) == OPTIONS)
			button.fsStyle |= BTNS_WHOLEDROPDOWN;
//#ifdef UNICODE
		//button.iString = (UINT_PTR)UnicodeText(ButtonText[i], S_ENC_DEFAULT);
//#else
		//button.iString = (UINT_PTR)ButtonText[i];
//#endif
		button.iString = (INT_PTR)SUcSwitch(ButtonText[i]);
		SendMessage(lpmw->hToolbar, TB_INSERTBUTTON, (WPARAM)i + 1, (LPARAM)&button);
//#ifdef UNICODE
		//SAlloc::F((LPWSTR)button.iString);
//#endif
	}
	// auto-resize and show 
	SendMessage(lpmw->hToolbar, TB_AUTOSIZE, (WPARAM)0, (LPARAM)0);
	ShowWindow(lpmw->hToolbar, SW_SHOWNOACTIVATE);
	// move top of client text window down to allow space for toolbar 
	GetClientRect(lpmw->hToolbar, &rect);
	lptw->ButtonHeight = rect.bottom - rect.top + 1;
	GetClientRect(lptw->hWndParent, &rect);
	SetWindowPos(lptw->hWndToolbar, NULL, 0, 0, rect.right, lptw->ButtonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(lptw->hWndText, (HWND)NULL, 0, lptw->ButtonHeight, rect.right, rect.bottom - lptw->ButtonHeight - lptw->StatusHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	goto cleanup;
nomemory:
	lptw->PopupError(L"Out of memory");
errorcleanup:
	if(hmacro) {
		GlobalUnlock(hmacro);
		GlobalFree(hmacro);
	}
	if(hmacrobuf) {
		GlobalUnlock(hmacrobuf);
		GlobalFree(hmacrobuf);
	}
	LocalFreePtr(lpmw->szPrompt);
	LocalFreePtr(lpmw->szAnswer);
cleanup:
	LocalFreePtr(buf);
	Gfclose(menufile);
}

void CloseMacros(TW * lptw)
{
	MW * lpmw = lptw->P_LpMw;
	HGLOBAL hglobal = (HGLOBAL)GlobalHandle(lpmw->macro);
	if(hglobal) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
	}
	hglobal = (HGLOBAL)GlobalHandle(lpmw->macrobuf);
	if(hglobal) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
	}
	LocalFreePtr(lpmw->szPrompt);
	LocalFreePtr(lpmw->szAnswer);
}
//
// InputBoxDlgProc() -  Message handling routine for Input dialog box  */
//
INT_PTR CALLBACK InputBoxDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) // @callback(DLGPROC)
{
	TW * lptw = (TW *)GetWindowLongPtr(GetParent(hDlg), 0);
	MW * lpmw = lptw->P_LpMw;
	switch(message) {
		case WM_INITDIALOG:
		    SetDlgItemTextW(hDlg, ID_PROMPT, lpmw->szPrompt);
		    return TRUE;
		case WM_COMMAND:
		    switch(LOWORD(wParam)) {
			    case ID_ANSWER:
					return TRUE;
			    case IDOK:
					lpmw->nChar = GetDlgItemTextW(hDlg, ID_ANSWER, lpmw->szAnswer, MAXSTR);
					EndDialog(hDlg, TRUE);
					return TRUE;
			    case IDCANCEL:
					lpmw->szAnswer[0] = 0;
					EndDialog(hDlg, FALSE);
					return TRUE;
			    default:
					return FALSE;
		    }
		default:
		    return FALSE;
	}
}
