#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include "StackFunc.h"
#include "StrFunc.h"
#include "WideFunc.h"
#include "AkelEdit.h"
#include "AkelDLL.h"
#include "Coder.h"
#include "HighLight.h"
#include "CodeFold.h"
#include "AutoComplete.h"
#include "Resources\Resource.h"


//// Global variables
SYNTAXFILE *lpSyntaxFileAutoComplete=NULL;
HDOCWORDS hDocWordsStack={0};
BLOCKINFO *lpCurrentBlockElement=NULL;
INT_PTR nWindowBlockBegin=0;
INT_PTR nWindowBlockEnd=0;
RECT rcAutoComplete={0, 0, 140, 102};
HWND hWndAutoComplete=NULL;
HWND hWndListBox=NULL;
WNDPROC OldListBoxProc=NULL;
HHOOK hHook=NULL;
BOOL bAutoListEnable=TRUE;
int nAutoListAfter=2;
DWORD dwCompleteWithList=544; //"Ctrl+Space"
DWORD dwCompleteNext=9;       //"Tab"
DWORD dwCompletePrev=0;       //"None"
BOOL bAddDocumentWords=TRUE;
BOOL bCompleteNonSyntaxDocument=TRUE;
BOOL bMaxDocumentEnable=TRUE;
int nMaxDocumentChars=1000000;
BOOL bAddHighLightWords=TRUE;
BOOL bRightDelimitersEnable=TRUE;
BOOL bSyntaxDelimitersEnable=TRUE;
BOOL bListShowOnlyMatched=TRUE;
BOOL bLockAutoList=FALSE;
BOOL bChangingText=FALSE;
BOOL bCompletingTitle=FALSE;
BOOL bFirstListBoxKey;
BLOCKINFO biHighLightBlock={0};
BLOCKINFO biDocWordBlock={0};
wchar_t wszHighLightTitle[MAX_PATH];
wchar_t wszDocWordTitle[MAX_PATH];
PLUGINFUNCTION *pfwCompleteWithList=NULL;
PLUGINFUNCTION *pfwCompleteNext=NULL;
PLUGINFUNCTION *pfwCompletePrev=NULL;


//Plugin extern function
void __declspec(dllexport) AutoComplete(PLUGINDATA *pd)
{
  pd->dwSupport|=PDS_SUPPORTALL;
  if (pd->dwSupport & PDS_GETSUPPORT)
    return;

  if (!bInitCommon)
    InitCommon(pd);
  else if (!bInitAutoComplete)
    ReadOptions(OF_AUTOCOMPLETE);

  if (pd->lParam)
  {
    INT_PTR nAction=GetExtCallParam(pd->lParam, 1);

    if (bInitAutoComplete)
    {
      if (nAction == DLLA_AUTOCOMPLETE_ADDWINDOW)
      {
        MANUALSET *lpManual;
        HWND hWndEdit=NULL;
        HWND hWndParent=NULL;
        unsigned char *pAlias=NULL;

        if (IsExtCallParamValid(pd->lParam, 2))
          hWndEdit=(HWND)GetExtCallParam(pd->lParam, 2);
        if (IsExtCallParamValid(pd->lParam, 3))
          pAlias=(unsigned char *)GetExtCallParam(pd->lParam, 3);
        hWndParent=GetParent(hWndEdit);

        if (hWndEdit && hWndParent && pAlias)
        {
          //Subclass window
          if (!(lpManual=StackGetManual(&hManualStack, hWndEdit, NULL)))
          {
            if (lpManual=StackInsertManual(&hManualStack))
            {
              lpManual->hWndEdit=hWndEdit;
              lpManual->hDocEdit=(AEHDOC)SendMessage(hWndEdit, AEM_GETDOCUMENT, 0, 0);
              lpManual->hWndParent=hWndParent;

              lpManual->lpOldEditProc=(WNDPROC)GetWindowLongPtrWide(hWndEdit, GWLP_WNDPROC);
              SetWindowLongPtrWide(hWndEdit, GWLP_WNDPROC, (UINT_PTR)NewUserEditProc);

              lpManual->lpOldParentProc=(WNDPROC)GetWindowLongPtrWide(hWndParent, GWLP_WNDPROC);
              SetWindowLongPtrWide(hWndParent, GWLP_WNDPROC, (UINT_PTR)NewUserParentProc);
            }
          }

          if (lpManual)
          {
            if (bOldWindows)
              MultiByteToWideChar(CP_ACP, 0, (char *)pAlias, -1, lpManual->wszAlias, MAX_PATH);
            else
              xstrcpynW(lpManual->wszAlias, (wchar_t *)pAlias, MAX_PATH);

            lpManual->dwDllFunction|=CODER_AUTOCOMPLETE;
          }
        }
      }
      else if (nAction == DLLA_AUTOCOMPLETE_DELWINDOW)
      {
        MANUALSET *lpManual;
        HWND hWndEdit=NULL;

        if (IsExtCallParamValid(pd->lParam, 2))
          hWndEdit=(HWND)GetExtCallParam(pd->lParam, 2);

        if (hWndEdit)
        {
          if (lpManual=StackGetManual(&hManualStack, hWndEdit, NULL))
            StackDeleteManual(&hManualStack, lpManual, CODER_AUTOCOMPLETE);
        }
      }
    }

    //If plugin already loaded, stay in memory and don't change active status
    if (pd->bInMemory) pd->nUnload=UD_NONUNLOAD_UNCHANGE;
    return;
  }

  if (bInitAutoComplete)
  {
    UninitMain();
    UninitAutoComplete();

    //If any function still loaded, stay in memory and show as non-active
    if (nInitMain) pd->nUnload=UD_NONUNLOAD_NONACTIVE;
  }
  else
  {
    InitMain();
    InitAutoComplete();

    //Stay in memory, and show as active
    pd->nUnload=UD_NONUNLOAD_ACTIVE;
  }
}

BOOL CALLBACK AutoCompleteSetupDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static HWND hWndAutoListEnable;
  static HWND hWndAutoListPreLabel;
  static HWND hWndAutoListPostLabel;
  static HWND hWndAutoListAfter;
  static HWND hWndAutoListAfterSpin;
  static HWND hWndCompleteWithList;
  static HWND hWndCompleteNext;
  static HWND hWndCompletePrev;
  static HWND hWndAddDocumentWords;
  static HWND hWndCompleteNonSyntaxDocument;
  static HWND hWndMaxDocumentEnable;
  static HWND hWndMaxDocumentChars;
  static HWND hWndMaxDocumentPostLabel;
  static HWND hWndAddHighLightWords;
  static HWND hWndRightDelimitersEnable;
  static HWND hWndSyntaxDelimitersEnable;
  static BOOL bInitDialog;

  if (uMsg == WM_INITDIALOG)
  {
    bInitDialog=TRUE;
    SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hMainIcon);
    hWndAutoListEnable=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLIST_ENABLE);
    hWndAutoListPreLabel=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER_PRELABEL);
    hWndAutoListPostLabel=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER_POSTLABEL);
    hWndAutoListAfter=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER);
    hWndAutoListAfterSpin=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER_SPIN);
    hWndCompleteWithList=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_COMPLETEWITHLIST);
    hWndCompleteNext=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_COMPLETENEXT);
    hWndCompletePrev=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_COMPLETEPREV);
    hWndAddDocumentWords=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_ADDDOCUMENTWORDS);
    hWndCompleteNonSyntaxDocument=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_COMPLETENONSYNTAXDOCUMENT);
    hWndMaxDocumentEnable=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_ENABLE);
    hWndMaxDocumentChars=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_CHARS);
    hWndMaxDocumentPostLabel=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_POSTLABEL);
    hWndAddHighLightWords=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_ADDHIGHLIGHTWORDS);
    hWndRightDelimitersEnable=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_RIGHTDELIMITERS_ENABLE);
    hWndSyntaxDelimitersEnable=GetDlgItem(hDlg, IDC_AUTOCOMPLETE_SETUP_SYNTAXDELIMITERS_ENABLE);

    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_GROUP, GetLangStringW(wLangModule, STRID_HOTKEYS));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_COMPLETEWITHLIST_LABEL, GetLangStringW(wLangModule, STRID_COMPLETEWITHLIST));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_COMPLETENEXT_LABEL, GetLangStringW(wLangModule, STRID_COMPLETENEXT));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_HOTKEY_COMPLETEPREV_LABEL, GetLangStringW(wLangModule, STRID_COMPLETEPREV));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLIST_GROUP, GetLangStringW(wLangModule, STRID_AUTOLIST));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLIST_ENABLE, GetLangStringW(wLangModule, STRID_ENABLE));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER_PRELABEL, GetLangStringW(wLangModule, STRID_AFTERCHAR_PRE));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER_POSTLABEL, GetLangStringW(wLangModule, STRID_AFTERCHAR_POST));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_DOCUMENTWORDS_GROUP, GetLangStringW(wLangModule, STRID_DOCUMENT));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_ADDDOCUMENTWORDS, GetLangStringW(wLangModule, STRID_ADDDOCUMENTWORDS));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_COMPLETENONSYNTAXDOCUMENT, GetLangStringW(wLangModule, STRID_COMPLETENONSYNTAXDOCUMENT));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_ENABLE, GetLangStringW(wLangModule, STRID_MAXDOCUMENT));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_POSTLABEL, GetLangStringW(wLangModule, STRID_CHARS));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_ADDHIGHLIGHTWORDS, GetLangStringW(wLangModule, STRID_ADDHIGHLIGHTWORDS));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_RIGHTDELIMITERS_ENABLE, GetLangStringW(wLangModule, STRID_RIGHTDELIMITERS));
    SetDlgItemTextWide(hDlg, IDC_AUTOCOMPLETE_SETUP_SYNTAXDELIMITERS_ENABLE, GetLangStringW(wLangModule, STRID_SYNTAXDELIMITERS));

    if (bAutoListEnable) SendMessage(hWndAutoListEnable, BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(hWndAutoListAfterSpin, UDM_SETBUDDY, (WPARAM)hWndAutoListAfter, 0);
    SendMessage(hWndAutoListAfterSpin, UDM_SETRANGE, 0, MAKELONG(99, 1));
    SetDlgItemInt(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER, nAutoListAfter, FALSE);

    if (bAddDocumentWords) SendMessage(hWndAddDocumentWords, BM_SETCHECK, BST_CHECKED, 0);
    if (bCompleteNonSyntaxDocument) SendMessage(hWndCompleteNonSyntaxDocument, BM_SETCHECK, BST_CHECKED, 0);
    if (bMaxDocumentEnable) SendMessage(hWndMaxDocumentEnable, BM_SETCHECK, BST_CHECKED, 0);
    SetDlgItemInt(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_CHARS, nMaxDocumentChars, FALSE);

    if (bAddHighLightWords) SendMessage(hWndAddHighLightWords, BM_SETCHECK, BST_CHECKED, 0);
    if (bRightDelimitersEnable) SendMessage(hWndRightDelimitersEnable, BM_SETCHECK, BST_CHECKED, 0);
    if (bSyntaxDelimitersEnable) SendMessage(hWndSyntaxDelimitersEnable, BM_SETCHECK, BST_CHECKED, 0);

    SendMessage(hWndCompleteWithList, HKM_SETHOTKEY, dwCompleteWithList, 0);
    SendMessage(hWndCompleteNext, HKM_SETHOTKEY, dwCompleteNext, 0);
    SendMessage(hWndCompletePrev, HKM_SETHOTKEY, dwCompletePrev, 0);
    SendMessage(hMainWnd, AKD_SETHOTKEYINPUT, (WPARAM)hWndCompleteWithList, pfwCompleteWithList?dwCompleteWithList:0);
    SendMessage(hMainWnd, AKD_SETHOTKEYINPUT, (WPARAM)hWndCompleteNext, pfwCompleteNext?dwCompleteNext:0);
    SendMessage(hMainWnd, AKD_SETHOTKEYINPUT, (WPARAM)hWndCompletePrev, pfwCompletePrev?dwCompletePrev:0);

    SendMessage(hDlg, WM_COMMAND, IDC_AUTOCOMPLETE_SETUP_AUTOLIST_ENABLE, 0);
    SendMessage(hDlg, WM_COMMAND, IDC_AUTOCOMPLETE_SETUP_ADDDOCUMENTWORDS, 0);
    bInitDialog=FALSE;
  }
  else if (uMsg == WM_COMMAND)
  {
    if (LOWORD(wParam) == IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER ||
        LOWORD(wParam) == IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_CHARS)
    {
      if (HIWORD(wParam) == EN_CHANGE)
      {
        SendMessage(hWndPropSheet, PSM_CHANGED, (WPARAM)hDlg, 0);
      }
    }
    else if (LOWORD(wParam) >= IDC_AUTOCOMPLETE_SETUP_AUTOLIST_GROUP &&
             LOWORD(wParam) <= IDC_AUTOCOMPLETE_SETUP_SYNTAXDELIMITERS_ENABLE)
    {
      if (!bInitDialog)
        SendMessage(hWndPropSheet, PSM_CHANGED, (WPARAM)hDlg, 0);
    }

    if (LOWORD(wParam) == IDC_AUTOCOMPLETE_SETUP_AUTOLIST_ENABLE)
    {
      bAutoListEnable=(BOOL)SendMessage(hWndAutoListEnable, BM_GETCHECK, 0, 0);
      EnableWindow(hWndAutoListAfter, bAutoListEnable);
      EnableWindow(hWndAutoListAfterSpin, bAutoListEnable);
      EnableWindow(hWndAutoListPreLabel, bAutoListEnable);
      EnableWindow(hWndAutoListPostLabel, bAutoListEnable);
    }
    else if (LOWORD(wParam) == IDC_AUTOCOMPLETE_SETUP_ADDDOCUMENTWORDS ||
             LOWORD(wParam) == IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_ENABLE)
    {
      bAddDocumentWords=(BOOL)SendMessage(hWndAddDocumentWords, BM_GETCHECK, 0, 0);
      bMaxDocumentEnable=(BOOL)SendMessage(hWndMaxDocumentEnable, BM_GETCHECK, 0, 0);
      EnableWindow(hWndCompleteNonSyntaxDocument, bAddDocumentWords);
      EnableWindow(hWndMaxDocumentEnable, bAddDocumentWords);
      EnableWindow(hWndMaxDocumentChars, bAddDocumentWords && bMaxDocumentEnable);
      EnableWindow(hWndMaxDocumentPostLabel, bAddDocumentWords && bMaxDocumentEnable);
    }
  }
  else if (uMsg == WM_NOTIFY)
  {
    if (((NMHDR *)lParam)->code == (UINT)PSN_SETACTIVE)
    {
      if (nPropMaxVisitPage < PAGE_AUTOCOMPLETE)
        nPropMaxVisitPage=PAGE_AUTOCOMPLETE;
    }
    else if (((NMHDR *)lParam)->code == (UINT)PSN_APPLY)
    {
      PSHNOTIFY *pshn=(PSHNOTIFY *)lParam;

      nAutoListAfter=GetDlgItemInt(hDlg, IDC_AUTOCOMPLETE_SETUP_AUTOLISTAFTER, NULL, FALSE);

      dwCompleteWithList=(WORD)SendMessage(hWndCompleteWithList, HKM_GETHOTKEY, 0, 0);
      dwCompleteNext=(WORD)SendMessage(hWndCompleteNext, HKM_GETHOTKEY, 0, 0);
      dwCompletePrev=(WORD)SendMessage(hWndCompletePrev, HKM_GETHOTKEY, 0, 0);

      bCompleteNonSyntaxDocument=(BOOL)SendMessage(hWndCompleteNonSyntaxDocument, BM_GETCHECK, 0, 0);
      nMaxDocumentChars=GetDlgItemInt(hDlg, IDC_AUTOCOMPLETE_SETUP_MAXDOCUMENT_CHARS, NULL, FALSE);

      bAddHighLightWords=(BOOL)SendMessage(hWndAddHighLightWords, BM_GETCHECK, 0, 0);
      bRightDelimitersEnable=(BOOL)SendMessage(hWndRightDelimitersEnable, BM_GETCHECK, 0, 0);
      bSyntaxDelimitersEnable=(BOOL)SendMessage(hWndSyntaxDelimitersEnable, BM_GETCHECK, 0, 0);

      if (bInitAutoComplete)
      {
        UninitAutoComplete();
        InitAutoComplete();
      }

      if (pshn->lParam)
      {
        //OK button pressed
        bSaveOptions=TRUE;
      }
      else
      {
        //Apply button pressed
        if (nPropMaxVisitPage == PAGE_AUTOCOMPLETE)
          UpdateAllOptions();
        return FALSE;
      }
    }
  }
  return FALSE;
}

BOOL CALLBACK AutoCompleteParentMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
  static BOOL bTextTypeChar;

  if (uMsg == WM_COMMAND)
  {
    if (LOWORD(wParam) == ID_EDIT || (HWND)*lResult)
    {
      if (HIWORD(wParam) == EN_CHANGE)
      {
        if (GetFocus() == (HWND)lParam)
        {
          CHARRANGE64 cr;

          hWndEdit=(HWND)lParam;
          SendMessage(hWndEdit, EM_EXGETSEL64, 0, (LPARAM)&cr);

          if (!bAkelEdit || bTextTypeChar)
          {
            if (hWndAutoComplete)
            {
              BLOCKINFO *lpBlockInfo=NULL;
              wchar_t wszTitlePart[MAX_PATH];
              int nIndex;
              BOOL bClose=TRUE;
              BOOL bEmpty=FALSE;
              BOOL bAddChar=FALSE;
              BOOL bDelChar=FALSE;
              BOOL bOnlyOne=FALSE;

              if (cr.cpMin == cr.cpMax)
              {
                if (nWindowBlockEnd == cr.cpMin - 1)
                  bAddChar=TRUE;
                if (nWindowBlockEnd == cr.cpMin + 1)
                  bDelChar=TRUE;
                if (nWindowBlockBegin == nWindowBlockEnd)
                  bEmpty=TRUE;

                if (bAddChar)
                {
                  if (GetEditTitlePart((lpSyntaxFileAutoComplete && bSyntaxDelimitersEnable)?&lpSyntaxFileAutoComplete->hDelimiterStack:NULL, wszTitlePart, MAX_PATH, &nWindowBlockBegin, &nWindowBlockEnd))
                  {
                    if (!bAutoListEnable || nWindowBlockEnd - nWindowBlockBegin >= nAutoListAfter)
                    {
                      if (wszTitlePart[0])
                      {
                        if (!(lpBlockInfo=StackGetBlock(lpSyntaxFileAutoComplete, &hDocWordsStack, wszTitlePart, (int)(nWindowBlockEnd - nWindowBlockBegin), &bOnlyOne)))
                        {
                          if (!bOnlyOne)
                            bLockAutoList=TRUE;
                        }
                      }
                      if (!wszTitlePart[0] || lpBlockInfo)
                      {
                        FillListbox(lpSyntaxFileAutoComplete, &hDocWordsStack, wszTitlePart);
                        nIndex=wszTitlePart[0]?0:-1;
                        SetSelListbox(nIndex);
                        bClose=FALSE;
                      }
                    }
                  }
                }
                if (bClose) SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
              }
            }
            else if (bAutoListEnable)
            {
              if (!bLockAutoList)
              {
                SYNTAXFILE *lpSyntaxFile;

                if (lpSyntaxFile=StackGetSyntaxFileByWindow(&hSyntaxFilesStack, hWndEdit, NULL, NULL))
                  StackRequestSyntaxFile(lpSyntaxFile);

                if (lpSyntaxFile || (bAddDocumentWords && bCompleteNonSyntaxDocument))
                {
                  if (CreateAutoCompleteWindow(lpSyntaxFile, CAW_AUTOLIST|CAW_COMPLETEWINDOW) == CAWE_GETBLOCK)
                    bLockAutoList=TRUE;
                }
              }
            }
          }
          else
          {
            if (hWndAutoComplete)
              SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
          }

          //Remove all spots if selection outside the spot
          if (lpCurrentBlockElement)
          {
            if (!GetHotSpot(lpCurrentBlockElement, hWndEdit, &cr))
            {
              StackResetHotSpot(lpCurrentBlockElement);
              lpCurrentBlockElement=NULL;
            }
          }
        }
      }
    }
  }
  else if (uMsg == WM_NOTIFY)
  {
    if (wParam == ID_EDIT || (HWND)*lResult)
    {
      if (((NMHDR *)lParam)->code == EN_SELCHANGE)
      {
        if (!bChangingText && !bCompletingTitle)
        {
          if (hWndAutoComplete)
            SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
          bLockAutoList=FALSE;
        }
      }
      else if (((NMHDR *)lParam)->code == AEN_TEXTCHANGING)
      {
        AENTEXTCHANGE *aentc=(AENTEXTCHANGE *)lParam;

        if (aentc->dwType & AETCT_CHAR)
        {
          bTextTypeChar=TRUE;
        }
        else
        {
          bTextTypeChar=FALSE;
          bLockAutoList=FALSE;
        }
        bChangingText=TRUE;
      }
      else if (((NMHDR *)lParam)->code == AEN_TEXTINSERTEND)
      {
        AENTEXTINSERT *aenti=(AENTEXTINSERT *)lParam;

        if (GetFocus() == aenti->hdr.hwndFrom)
        {
          hWndEdit=aenti->hdr.hwndFrom;

          //Check for unlock auto-list
          if (aenti->dwType & AETCT_CHAR)
          {
            if (bLockAutoList)
            {
              SYNTAXFILE *lpSyntaxFile;

              lpSyntaxFile=StackGetSyntaxFileByWindow(&hSyntaxFilesStack, hWndEdit, NULL, NULL);
              if (IsDelimiter((lpSyntaxFile && bSyntaxDelimitersEnable)?&lpSyntaxFile->hDelimiterStack:NULL, hWndEdit, *aenti->wpText))
                bLockAutoList=FALSE;
            }
          }

          //Update hotspots
          if (lpCurrentBlockElement)
          {
            HOTSPOT *lpHotSpot=NULL;
            CHARRANGE64 cr;

            if (aenti->crRichRange.cpMin != aenti->crRichRange.cpMax)
            {
              cr.cpMin=aenti->crRichRange.cpMin;
              cr.cpMax=aenti->crRichRange.cpMin;

              if (lpHotSpot=GetHotSpot(lpCurrentBlockElement, hWndEdit, &cr))
              {
                lpHotSpot->nHotSpotLen+=(int)(aenti->crRichRange.cpMax - aenti->crRichRange.cpMin);

                while (lpHotSpot=lpHotSpot->next)
                {
                  if (aenti->crRichRange.cpMin <= lpCurrentBlockElement->nHotSpotBlockBegin + lpHotSpot->nHotSpotPos)
                  {
                    lpHotSpot->nHotSpotPos+=(int)(aenti->crRichRange.cpMax - aenti->crRichRange.cpMin);
                  }
                }
              }
              else
              {
                StackResetHotSpot(lpCurrentBlockElement);
                lpCurrentBlockElement=NULL;
              }
            }
          }
        }
      }
      else if (((NMHDR *)lParam)->code == AEN_TEXTDELETEBEGIN)
      {
        //Update hotspots
        if (lpCurrentBlockElement)
        {
          AENTEXTDELETE *aentd=(AENTEXTDELETE *)lParam;
          HOTSPOT *lpHotSpot=NULL;

          if (aentd->crRichRange.cpMin != aentd->crRichRange.cpMax)
          {
            if (lpHotSpot=GetHotSpot(lpCurrentBlockElement, aentd->hdr.hwndFrom, &aentd->crRichRange))
            {
              lpHotSpot->nHotSpotLen-=(int)(aentd->crRichRange.cpMax - aentd->crRichRange.cpMin);

              while (lpHotSpot=lpHotSpot->next)
              {
                if (aentd->crRichRange.cpMin <= lpCurrentBlockElement->nHotSpotBlockBegin + lpHotSpot->nHotSpotPos)
                {
                  lpHotSpot->nHotSpotPos-=(int)(aentd->crRichRange.cpMax - aentd->crRichRange.cpMin);
                }
              }
            }
            else
            {
              StackResetHotSpot(lpCurrentBlockElement);
              lpCurrentBlockElement=NULL;
            }
          }
        }
      }
      else if (((NMHDR *)lParam)->code == AEN_TEXTCHANGED)
      {
        bChangingText=FALSE;
      }
    }
  }
  return FALSE;
}

BOOL CALLBACK AutoCompleteEditMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *lResult)
{
  if (uMsg == WM_KILLFOCUS)
  {
    if (hWndAutoComplete)
      SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);

    if (lpCurrentBlockElement)
    {
      StackResetHotSpot(lpCurrentBlockElement);
      lpCurrentBlockElement=NULL;
    }
  }
  return FALSE;
}

LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (code >= 0)
  {
    MSG *msg=(MSG *)lParam;

    if (hWndAutoComplete)
    {
      if (msg->message == WM_LBUTTONDOWN ||
          msg->message == WM_NCLBUTTONDOWN)
      {
        RECT rc;
        POINT pt;

        GetWindowRect(hWndAutoComplete, &rc);
        GetCursorPos(&pt);
        if (!PtInRect(&rc, pt))
          SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
      }
    }
    if (hWndListBox)
    {
      if (msg->message == WM_MOUSEWHEEL)
      {
        SendMessage(hWndListBox, msg->message, msg->wParam, msg->lParam);
        msg->message=WM_NULL;
      }
      else if (msg->message == WM_KEYDOWN ||
               msg->message == WM_KEYUP)
      {
        if (msg->wParam == VK_UP ||
            msg->wParam == VK_DOWN ||
            msg->wParam == VK_PRIOR ||
            msg->wParam == VK_NEXT)
        {
          if (bFirstListBoxKey && (msg->wParam == VK_UP ||
                                   GetKeyState(VK_MENU) < 0 ||
                                   GetKeyState(VK_SHIFT) < 0 ||
                                   GetKeyState(VK_CONTROL) < 0))
          {
            if (hWndAutoComplete) SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
          }
          else
          {
            bFirstListBoxKey=FALSE;

            if (bOldWindows)
            {
              SendMessage(hWndListBox, WM_SETREDRAW, FALSE, 0);
            }

            SendMessage(hWndListBox, msg->message, msg->wParam, msg->lParam);
            msg->message=WM_NULL;

            if (bOldWindows)
            {
              SendMessage(hWndListBox, WM_SETREDRAW, TRUE, 0);
              InvalidateRect(hWndListBox, NULL, TRUE);
            }
          }
        }
        else
        {
          if (msg->message == WM_KEYDOWN)
          {
            if (msg->wParam == VK_ESCAPE ||
                msg->wParam == VK_RETURN)
            {
              if (msg->wParam == VK_RETURN)
              {
                if (GetKeyState(VK_MENU) >= 0 &&
                    GetKeyState(VK_SHIFT) >= 0 &&
                    GetKeyState(VK_CONTROL) >= 0)
                {
                  BLOCKINFO *lpBlockInfo;

                  if (lpBlockInfo=GetBlockListbox())
                  {
                    CompleteTitlePart(lpBlockInfo, nWindowBlockBegin, nWindowBlockEnd);
                    msg->message=WM_NULL;
                  }
                }
              }
              else msg->message=WM_NULL;

              if (hWndAutoComplete) SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
            }
          }
        }
      }
    }
  }
  return CallNextHookEx(hHook, code, wParam, lParam);
}

BOOL CALLBACK CompleteWithListProc(void *lpParameter, LPARAM lParam, DWORD dwSupport)
{
  SYNTAXFILE *lpSyntaxFile;
  DWORD dwCreateFlags=0;
  BOOL bCatchHotkey=TRUE;

  if (hWndEdit=GetFocusEdit())
  {
    if (!lpParameter)
      dwCreateFlags=CAW_COMPLETEONE|CAW_COMPLETEWINDOW;
    else
      dwCreateFlags=(DWORD)(UINT_PTR)lpParameter;

    if (hWndAutoComplete)
      SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);

    if (lpSyntaxFile=StackGetSyntaxFileByWindow(&hSyntaxFilesStack, hWndEdit, NULL, NULL))
      StackRequestSyntaxFile(lpSyntaxFile);

    if (lpSyntaxFile || (bAddDocumentWords && bCompleteNonSyntaxDocument))
    {
      if (CreateAutoCompleteWindow(lpSyntaxFile, dwCreateFlags))
      {
        if (!(dwCreateFlags & CAW_COMPLETEWINDOW))
          bCatchHotkey=FALSE;
      }
    }
    else if (!(dwCreateFlags & CAW_COMPLETEWINDOW))
      bCatchHotkey=FALSE;
  }
  return bCatchHotkey;
}

BOOL CALLBACK CompleteNextProc(void *lpParameter, LPARAM lParam, DWORD dwSupport)
{
  BOOL bCatchHotkey=FALSE;

  if (hWndEdit=GetFocusEdit())
  {
    if (lpCurrentBlockElement)
    {
      if (!NextHotSpot(lpCurrentBlockElement, hWndEdit, FALSE))
      {
        StackResetHotSpot(lpCurrentBlockElement);
        lpCurrentBlockElement=NULL;
      }
      else bCatchHotkey=TRUE;
    }
    else
    {
      bCatchHotkey=CompleteWithListProc((void *)(UINT_PTR)(CAW_COMPLETEONE|CAW_COMPLETEEXACT), 0, 0);
    }
  }
  return bCatchHotkey;
}

BOOL CALLBACK CompletePrevProc(void *lpParameter, LPARAM lParam, DWORD dwSupport)
{
  BOOL bCatchHotkey=FALSE;

  if (hWndEdit=GetFocusEdit())
  {
    if (lpCurrentBlockElement)
    {
      if (!NextHotSpot(lpCurrentBlockElement, hWndEdit, TRUE))
      {
        StackResetHotSpot(lpCurrentBlockElement);
        lpCurrentBlockElement=NULL;
      }
      else bCatchHotkey=TRUE;
    }
  }
  return bCatchHotkey;
}

LRESULT CALLBACK AutoCompleteWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static POINT ptResizeHitOffset={0};
  static int nResizeHit=0;

  if (uMsg == WM_CREATE)
  {
    HFONT hListboxFont;

    hWndListBox=CreateWindowExWide(0,
                            L"LISTBOX",
                            NULL,
                            WS_CHILD|WS_VSCROLL|WS_VISIBLE|
                            LBS_DISABLENOSCROLL|LBS_NOINTEGRALHEIGHT|LBS_SORT,
                            0, 0, 0, 0,
                            hWnd,
                            (HMENU)(UINT_PTR)IDC_LIST,
                            hInstanceDLL,
                            NULL);

    hListboxFont=(HFONT)SendMessage(hWndEdit, AEM_GETFONT, AEGF_CURRENT, 0);
    SendMessage(hWndListBox, WM_SETFONT, (WPARAM)hListboxFont, TRUE);

    OldListBoxProc=(WNDPROC)GetWindowLongPtrWide(hWndListBox, GWLP_WNDPROC);
    SetWindowLongPtrWide(hWndListBox, GWLP_WNDPROC, (UINT_PTR)NewListboxProc);
  }
  else if (uMsg == WM_SIZE)
  {
    if (hWndListBox)
    {
      SetWindowPos(hWndListBox, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);
    }
    return FALSE;
  }
  else if (uMsg == WM_MOUSEACTIVATE)
  {
    return MA_NOACTIVATE;
  }
  else if (uMsg == WM_NCHITTEST)
  {
    LRESULT lResult;

    lResult=DefWindowProcWide(hWnd, uMsg, wParam, lParam);

    if (lResult == HTRIGHT ||
        lResult == HTBOTTOMRIGHT ||
        lResult == HTBOTTOM)
    {
      return lResult;
    }
    return HTERROR;
  }
  else if (uMsg == WM_NCLBUTTONDOWN)
  {
    RECT rc;
    POINT pt;

    GetWindowRect(hWnd, &rc);
    GetCursorPos(&pt);

    if (wParam == HTRIGHT ||
        wParam == HTBOTTOMRIGHT ||
        wParam == HTBOTTOM)
    {
      ptResizeHitOffset.x=rc.right - pt.x;
      ptResizeHitOffset.y=rc.bottom - pt.y;
    }
    else if (wParam == HTTOPRIGHT)
    {
      ptResizeHitOffset.x=rc.right - pt.x;
      ptResizeHitOffset.y=rc.top - pt.y;
    }
    else if (wParam == HTLEFT ||
             wParam == HTTOPLEFT ||
             wParam == HTTOP)
    {
      ptResizeHitOffset.x=rc.left - pt.x;
      ptResizeHitOffset.y=rc.top - pt.y;
    }
    else if (wParam == HTBOTTOMLEFT)
    {
      ptResizeHitOffset.x=rc.left - pt.x;
      ptResizeHitOffset.y=rc.bottom - pt.y;
    }
    SetCapture(hWnd);
    nResizeHit=(int)wParam;
    return FALSE;
  }
  else if (uMsg == WM_MOUSEMOVE)
  {
    if (nResizeHit)
    {
      RECT rc;
      POINT pt;

      GetWindowRect(hWnd, &rc);
      GetCursorPos(&pt);

      pt.x+=ptResizeHitOffset.x;
      pt.y+=ptResizeHitOffset.y;

      if (nResizeHit == HTLEFT)
      {
        rc.left=pt.x;
      }
      else if (nResizeHit == HTRIGHT)
      {
        rc.right=pt.x;
      }
      else if (nResizeHit == HTTOP)
      {
        rc.top=pt.y;
      }
      else if (nResizeHit == HTTOPLEFT)
      {
        rc.top=pt.y;
        rc.left=pt.x;
      }
      else if (nResizeHit == HTTOPRIGHT)
      {
        rc.top=pt.y;
        rc.right=pt.x;
      }
      else if (nResizeHit == HTBOTTOM)
      {
        rc.bottom=pt.y;
      }
      else if (nResizeHit == HTBOTTOMLEFT)
      {
        rc.bottom=pt.y;
        rc.left=pt.x;
      }
      else if (nResizeHit == HTBOTTOMRIGHT)
      {
        rc.bottom=pt.y;
        rc.right=pt.x;
      }
      SetWindowPos(hWnd, NULL, rc.left, rc.top, max(rc.right - rc.left, SIZE_MINX), max(rc.bottom - rc.top, SIZE_MINY), SWP_NOZORDER|SWP_NOACTIVATE);
      return FALSE;
    }
  }
  else if (uMsg == WM_LBUTTONUP ||
           uMsg == WM_CANCELMODE)
  {
    if (nResizeHit)
    {
      nResizeHit=0;
      ReleaseCapture();
    }
  }
  else if (uMsg == WM_CLOSE)
  {
    RECT rc;

    GetWindowRect(hWnd, &rc);
    rc.right-=rc.left;
    rc.bottom-=rc.top;
    if (rcAutoComplete.right != rc.right ||
        rcAutoComplete.bottom != rc.bottom)
    {
      rcAutoComplete=rc;
      dwSaveFlags|=OF_AUTOCOMPLETE_RECT;
    }
    CloseAutoCompleteWindow();
  }
  else if (uMsg == WM_DESTROY)
  {
    hWndAutoComplete=NULL;
    hWndListBox=NULL;
  }

  return DefWindowProcWide(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK NewListboxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_MOUSEACTIVATE)
  {
    return MA_NOACTIVATE;
  }
  else if (uMsg == WM_LBUTTONDOWN)
  {
    LRESULT lResult=SendMessage(hWnd, LB_ITEMFROMPOINT, 0, lParam);

    if (HIWORD(lResult) == 0)
    {
      SendMessage(hWnd, LB_SETCURSEL, LOWORD(lResult), 0);
    }
    return FALSE;
  }
  else if (uMsg == WM_LBUTTONUP)
  {
    return FALSE;
  }
  else if (uMsg == WM_MBUTTONDOWN)
  {
    return FALSE;
  }
  else if (uMsg == WM_MBUTTONUP)
  {
    return FALSE;
  }
  else if (uMsg == WM_LBUTTONDBLCLK)
  {
    BLOCKINFO *lpBlockInfo;

    if (lpBlockInfo=GetBlockListbox())
      CompleteTitlePart(lpBlockInfo, nWindowBlockBegin, nWindowBlockEnd);
    SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
    return FALSE;
  }

  if (bOldWindows)
    return CallWindowProcA(OldListBoxProc, hWnd, uMsg, wParam, lParam);
  else
    return CallWindowProcW(OldListBoxProc, hWnd, uMsg, wParam, lParam);
}

DWORD CreateAutoCompleteWindow(SYNTAXFILE *lpSyntaxFile, DWORD dwFlags)
{
  BLOCKINFO *lpBlockInfo=NULL;
  wchar_t wszTitlePart[MAX_PATH];
  int nIndex;
  INT_PTR nBlockBegin;
  INT_PTR nBlockEnd;
  BOOL bOnlyOne=FALSE;
  BOOL bSkipAddDocumentWords=FALSE;

  if (hWndEdit=GetFocusEdit())
  {
    if (bAddDocumentWords && bMaxDocumentEnable && nMaxDocumentChars)
    {
      GETTEXTLENGTHEX gtl;
      INT_PTR nTextLength;

      gtl.flags=GTL_DEFAULT;
      nTextLength=SendMessage(hWndEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
      if (nMaxDocumentChars < nTextLength)
      {
        if (!lpSyntaxFile)
          return CAWE_DOCUMENTLIMIT;
        bSkipAddDocumentWords=TRUE;
      }
    }
  }
  else return CAWE_NOEDITWINDOW;

  if (lpSyntaxFile && lpSyntaxFile->hExactBlockStack.first && (dwFlags & CAW_COMPLETEONE))
  {
    AECHARINDEX ciCaret;
    INT_PTR nCaretOffset;

    SendMessage(hWndEdit, AEM_GETINDEX, AEGI_CARETCHAR, (LPARAM)&ciCaret);
    nCaretOffset=SendMessage(hWndEdit, AEM_GETRICHOFFSET, AEGI_CARETCHAR, 0);
    if (lpBlockInfo=StackGetExactBlock(lpSyntaxFile, &ciCaret, nCaretOffset, &nBlockBegin, &nBlockEnd))
    {
      CompleteTitlePart(lpBlockInfo, nBlockBegin, nBlockEnd);
      return CAWE_SUCCESS;
    }
  }

  if (!GetEditTitlePart((lpSyntaxFile && bSyntaxDelimitersEnable)?&lpSyntaxFile->hDelimiterStack:NULL, wszTitlePart, MAX_PATH, &nBlockBegin, &nBlockEnd))
    return CAWE_GETTITLEPART;

  if (dwFlags & CAW_AUTOLIST)
  {
    if (nBlockEnd - nBlockBegin < nAutoListAfter)
      return CAWE_AUTOLIST;
  }

  //Complete without window
  if (wszTitlePart[0])
  {
    if (bAddDocumentWords && !bSkipAddDocumentWords)
      StackFillDocWord((lpSyntaxFile && bSyntaxDelimitersEnable)?&lpSyntaxFile->hDelimiterStack:NULL, &hDocWordsStack, wszTitlePart, (int)(nBlockEnd - nBlockBegin));

    if (lpBlockInfo=StackGetBlock(lpSyntaxFile, &hDocWordsStack, wszTitlePart, (int)(nBlockEnd - nBlockBegin), &bOnlyOne))
    {
      if (bOnlyOne || ((dwFlags & CAW_COMPLETEEXACT) && !xstrcmpiW(lpBlockInfo->wpTitle, wszTitlePart)))
      {
        if (dwFlags & CAW_COMPLETEONE)
        {
          CompleteTitlePart(lpBlockInfo, nBlockBegin, nBlockEnd);
          StackFreeDocWord(&hDocWordsStack);
          return CAWE_SUCCESS;
        }
      }
    }
    else
    {
      StackFreeDocWord(&hDocWordsStack);
      return CAWE_GETBLOCK;
    }
  }

  //Complete with window
  if (dwFlags & CAW_COMPLETEWINDOW)
  {
    if (!wszTitlePart[0] || lpBlockInfo)
    {
      hWndAutoComplete=CreateWindowExWide(0,
                            AUTOCOMPLETEW,
                            NULL,
                            WS_POPUP|WS_THICKFRAME,
                            0, 0, rcAutoComplete.right, rcAutoComplete.bottom,
                            hMainWnd,
                            NULL,
                            hInstanceDLL,
                            NULL);

      lpSyntaxFileAutoComplete=lpSyntaxFile;
      nWindowBlockBegin=nBlockBegin;
      nWindowBlockEnd=nBlockEnd;

      if (MoveAutoCompleteWindow())
      {
        FillListbox(lpSyntaxFile, &hDocWordsStack, wszTitlePart);
        nIndex=wszTitlePart[0]?0:-1;
        SetSelListbox(nIndex);
        ShowWindow(hWndAutoComplete, SW_SHOWNOACTIVATE);

        bFirstListBoxKey=TRUE;
        hHook=SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, NULL, GetWindowThreadProcessId(hWndEdit, NULL));
        return CAWE_SUCCESS;
      }
      else SendMessage(hWndAutoComplete, WM_CLOSE, 0, 0);
    }
  }
  return CAWE_OTHER;
}

BOOL MoveAutoCompleteWindow()
{
  RECT rc;
  POINT pt;
  int nCharHeight;

  if (!hWndAutoComplete) return FALSE;
  GetPosFromChar(hWndEdit, nWindowBlockBegin, &pt);
  nCharHeight=(int)SendMessage(hWndEdit, AEM_GETCHARSIZE, AECS_HEIGHT, 0);
  pt.y+=nCharHeight;

  if (ClientToScreen(hWndEdit, &pt))
  {
    GetWindowRect(hWndEdit, &rc);

    if (pt.x + rcAutoComplete.right > rc.right &&
        pt.x - rcAutoComplete.right >= rc.left)
    {
      pt.x=pt.x - rcAutoComplete.right;
    }
    if (pt.y + rcAutoComplete.bottom > rc.bottom &&
        pt.y - rcAutoComplete.bottom - nCharHeight >= rc.top)
    {
      pt.y=pt.y - rcAutoComplete.bottom - nCharHeight;
    }
    if (pt.x >= 0 && pt.y >= 0 && pt.x < rc.right && pt.y < rc.bottom)
    {
      SetWindowPos(hWndAutoComplete, NULL, pt.x, pt.y, 0, 0, SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
      return TRUE;
    }
  }
  return FALSE;
}

void CloseAutoCompleteWindow()
{
  if (hHook)
  {
    if (UnhookWindowsHookEx(hHook))
      hHook=NULL;
  }
  if (bAddDocumentWords)
    StackFreeDocWord(&hDocWordsStack);
}

void FillListbox(SYNTAXFILE *lpSyntaxFile, HDOCWORDS *hDocWordsStack, const wchar_t *wpTitlePart)
{
  BLOCKINFO *lpBlockElement;
  int nItem;

  SendMessage(hWndListBox, LB_RESETCONTENT, 0, 0);

  if (lpSyntaxFile)
  {
    //Use ListBox_AddString to sort words from HighLight base
    if (bAddHighLightWords)
    {
      WORDINFO *lpWordElement=(WORDINFO *)lpSyntaxFile->hWordStack.first;
      int nItem;

      while (lpWordElement)
      {
        if (!xstrcmpinW(wpTitlePart, lpWordElement->wpWord, (UINT_PTR)-1))
        {
          xprintfW(wszBuffer, L"%s*", lpWordElement->wpWord);
          if ((nItem=ListBox_AddStringWide(hWndListBox, wszBuffer)) != LB_ERR)
            SendMessage(hWndListBox, LB_SETITEMDATA, nItem, (LPARAM)lpWordElement);
        }
        lpWordElement=lpWordElement->next;
      }
    }

    //Use hBlockStack.last because ListBox_InsertString to zero index reverse items order
    lpBlockElement=(BLOCKINFO *)lpSyntaxFile->hBlockStack.last;

    while (lpBlockElement)
    {
      if (!xstrcmpinW(wpTitlePart, lpBlockElement->wpTitle, (UINT_PTR)-1))
      {
        if ((nItem=ListBox_InsertStringWide(hWndListBox, 0, lpBlockElement->wpTitle)) != LB_ERR)
          SendMessage(hWndListBox, LB_SETITEMDATA, nItem, (LPARAM)lpBlockElement);
      }
      lpBlockElement=lpBlockElement->prev;
    }

    if (!wpTitlePart[0])
    {
      //Use hBlockStack.last because ListBox_InsertString to zero index reverse items order
      lpBlockElement=(BLOCKINFO *)lpSyntaxFile->hExactBlockStack.last;
  
      while (lpBlockElement)
      {
        if ((nItem=ListBox_InsertStringWide(hWndListBox, 0, lpBlockElement->wpTitle)) != LB_ERR)
          SendMessage(hWndListBox, LB_SETITEMDATA, nItem, (LPARAM)lpBlockElement);

        lpBlockElement=lpBlockElement->prev;
      }
    }
  }

  //Document words
  if (bAddDocumentWords)
  {
    DOCWORDINFO *lpDocWordInfo=hDocWordsStack->first;
    int nItem;

    while (lpDocWordInfo)
    {
      if (!xstrcmpinW(wpTitlePart, lpDocWordInfo->wpDocWord, (UINT_PTR)-1))
      {
        xprintfW(wszBuffer, L"%s+", lpDocWordInfo->wpDocWord);
        if ((nItem=ListBox_InsertStringWide(hWndListBox, -1, wszBuffer)) != LB_ERR)
          SendMessage(hWndListBox, LB_SETITEMDATA, nItem, (LPARAM)lpDocWordInfo);
      }
      lpDocWordInfo=lpDocWordInfo->next;
    }
  }
}

void SetSelListbox(int nIndex)
{
  if (bOldWindows)
  {
    SendMessage(hWndListBox, WM_SETREDRAW, FALSE, 0);
  }
  if (nIndex != -1)
  {
    SendMessage(hWndListBox, LB_SETCURSEL, nIndex, 0);
    SendMessage(hWndListBox, LB_SETTOPINDEX, nIndex, 0);
  }
  else
  {
    SendMessage(hWndListBox, LB_SETCURSEL, 0, 0);
    SendMessage(hWndListBox, LB_SETCURSEL, (WPARAM)-1, 0);
    SendMessage(hWndListBox, LB_SETTOPINDEX, 0, 0);
  }
  if (bOldWindows)
  {
    SendMessage(hWndListBox, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hWndListBox, NULL, TRUE);
  }
}

BLOCKINFO* GetBlockListbox()
{
  BLOCKINFO *lpBlock;
  WORDINFO *lpHighLightWord;
  DOCWORDINFO *lpDocWord;
  int nItem;

  if ((nItem=(int)SendMessage(hWndListBox, LB_GETCURSEL, 0, 0)) != LB_ERR)
  {
    lpBlock=(BLOCKINFO *)SendMessage(hWndListBox, LB_GETITEMDATA, nItem, 0);

    //Item data is BLOCKINFO pointer
    if (lpBlock->nStructType == BIT_BLOCK)
      return lpBlock;

    //Item data is DOCWORDINFO pointer
    if (lpBlock->nStructType == BIT_DOCWORD)
    {
      lpDocWord=(DOCWORDINFO *)lpBlock;
      biDocWordBlock.nTitleLen=(int)xprintfW(wszDocWordTitle, L"%s+", lpDocWord->wpDocWord);
      biDocWordBlock.wpTitle=wszDocWordTitle;
      biDocWordBlock.wpBlock=lpDocWord->wpDocWord;
      biDocWordBlock.nBlockLen=lpDocWord->nDocWordLen;
      biDocWordBlock.nLinesInBlock=1;
      return &biDocWordBlock;
    }

    //Item data is WORDINFO pointer
    lpHighLightWord=(WORDINFO *)lpBlock;
    biHighLightBlock.nTitleLen=(int)xprintfW(wszHighLightTitle, L"%s*", lpHighLightWord->wpWord);
    biHighLightBlock.wpTitle=wszHighLightTitle;
    biHighLightBlock.wpBlock=lpHighLightWord->wpWord;
    biHighLightBlock.nBlockLen=lpHighLightWord->nWordLen;
    biHighLightBlock.nLinesInBlock=1;
    return &biHighLightBlock;
  }
  return NULL;
}

int ParseBlock(SYNTAXFILE *lpScheme, HSTACK *hHotSpotStack, const wchar_t *wpInput, int nInputLen, wchar_t *wszOutput, int *nOutputLines)
{
  VARINFO *lpElement;
  HOTSPOT *lpHotSpot=NULL;
  const wchar_t *wpVarNameStart;
  int nLines=1;
  int nVarNameLen;
  int a;
  int b;

  for (a=0, b=0; b < nInputLen; ++b)
  {
    if (wpInput[b] == '$')
    {
      if (wpInput[++b] == ';')
      {
        while (++b < nInputLen && wpInput[b] != '\r' && wpInput[b] != '\0');
      }
      else if (wpInput[b] == '$')
      {
        if (wszOutput) wszOutput[a]='$';
        ++a;
        if (lpHotSpot) ++lpHotSpot->nHotSpotInitLen;
      }
      else if (wpInput[b] == '\\')
      {
        if (++b >= nInputLen) break;
        if (wszOutput) wszOutput[a]=wpInput[b];
        ++a;
        if (lpHotSpot) ++lpHotSpot->nHotSpotInitLen;
      }
      else if (wpInput[b] == '[')
      {
        if (wszOutput) lpHotSpot=StackInsertHotSpot(hHotSpotStack, a);
      }
      else if (wpInput[b] == '{')
      {
        wpVarNameStart=wpInput + b + 1;
        for (nVarNameLen=0; ++b < nInputLen && wpInput[b] != '}'; ++nVarNameLen);
        if (b >= nInputLen) break;

        if (lpElement=StackGetVarByName(&lpVarThemeActive->hVarStack, wpVarNameStart, nVarNameLen))
        {
          if (wszOutput) xmemcpy(wszOutput + a, lpElement->wpVarValue, lpElement->nVarValueLen * sizeof(wchar_t));
          a+=lpElement->nVarValueLen;
          if (lpHotSpot) lpHotSpot->nHotSpotInitLen+=lpElement->nVarValueLen;
        }
      }
      continue;
    }
    else if (wpInput[b] == '\n')
    {
      //Skip '\n'
      continue;
    }
    else if (wpInput[b] == '\r')
    {
      ++nLines;
    }
    else if (wpInput[b] == ']')
    {
      if (lpHotSpot)
      {
        lpHotSpot->nHotSpotLen=lpHotSpot->nHotSpotInitLen;
        lpHotSpot=NULL;
        continue;
      }
    }
    if (wszOutput) wszOutput[a]=wpInput[b];
    ++a;
    if (lpHotSpot) ++lpHotSpot->nHotSpotInitLen;
  }
  if (wszOutput)
    wszOutput[a]='\0';
  else
    ++a;
  if (nOutputLines) *nOutputLines=nLines;
  return a;
}

TITLEINFO* StackInsertTitle(STACKTITLE *hStack)
{
  TITLEINFO *lpElement=NULL;

  StackInsertIndex((stack **)&hStack->first, (stack **)&hStack->last, (stack **)&lpElement, -1, sizeof(TITLEINFO));
  return lpElement;
}

void StackFreeTitle(STACKTITLE *hStack)
{
  StackClear((stack **)&hStack->first, (stack **)&hStack->last);
}

BOOL GetEditTitlePart(STACKDELIM *hDelimiterStack, wchar_t *wszTitle, int nTitleMax, INT_PTR *nMin, INT_PTR *nMax)
{
  AETEXTRANGEW tr;
  AESELECTION aes;
  AECHARINDEX ciCaret;
  AECHARINDEX ciCount;
  CHARRANGE64 cr;
  int nTitleLen;

  if (!SendMessage(hWndEdit, AEM_GETSEL, (WPARAM)&ciCaret, (LPARAM)&aes) && ciCaret.nCharInLine <= ciCaret.lpLine->nLineLen)
  {
    //Check right delimiter
    if (bRightDelimitersEnable)
    {
      if (!IsDelimiterFromRight(hDelimiterStack, hWndEdit, &aes.crSel.ciMin))
        return FALSE;
    }
    SendMessage(hWndEdit, EM_EXGETSEL64, 0, (LPARAM)&cr);
    nTitleLen=0;
    ciCount=aes.crSel.ciMin;

    while (AEC_PrevCharInLine(&ciCount))
    {
      if (IsDelimiter(hDelimiterStack, hWndEdit, *(ciCount.lpLine->wpLine + ciCount.nCharInLine)))
      {
        AEC_NextCharInLine(&ciCount);
        break;
      }
      ++nTitleLen;
    }

    if (nTitleLen)
    {
      tr.cr.ciMin=ciCount;
      tr.cr.ciMax=aes.crSel.ciMax;
      tr.bColumnSel=FALSE;
      tr.pBuffer=wszTitle;
      tr.dwBufferMax=nTitleMax;
      tr.nNewLine=AELB_ASOUTPUT;
      tr.bFillSpaces=FALSE;
      nTitleLen=(int)SendMessage(hWndEdit, AEM_GETTEXTRANGEW, 0, (LPARAM)&tr);

      if (nTitleLen + 1 < nTitleMax)
      {
        *nMin=cr.cpMin - nTitleLen;
        *nMax=cr.cpMin;
        return TRUE;
      }
    }

    //Empty word
    wszTitle[0]='\0';
    *nMin=cr.cpMin;
    *nMax=cr.cpMin;
    return TRUE;
  }
  return FALSE;
}

void CompleteTitlePart(BLOCKINFO *lpBlockInfo, INT_PTR nMin, INT_PTR nMax)
{
  AEFINDTEXTW ft;
  AECHARINDEX ciChar;
  GETTEXTRANGE gtr;
  CHARRANGE64 cr;
  BLOCKINFO *lpBlockMaster;
  BLOCKINFOHANDLE *lpBlockHandle;
  HOTSPOT *lpHotSpot;
  wchar_t *wpStrBegin;
  wchar_t *wpStrEnd;
  wchar_t *wpLine;
  wchar_t *wpIndentBlock=NULL;
  int nMinLine;
  INT_PTR nMinLineIndex;
  int nLen;
  int nSpaceCount=0;
  int a;
  int b;

  if (hWndEdit=GetFocusEdit())
  {
    if (!(SendMessage(hWndEdit, AEM_GETOPTIONS, 0, 0) & AECO_READONLY))
    {
      bCompletingTitle=TRUE;

      if (bAkelEdit)
      {
        if (lpCurrentBlockElement)
          StackResetHotSpot(lpCurrentBlockElement);
        lpCurrentBlockElement=NULL;
      }
      if (lpBlockInfo->master)
        lpBlockMaster=lpBlockInfo->master;
      else
        lpBlockMaster=lpBlockInfo;

      //Smart complete multiple abbreviations, like $~GetAkelDir $~AkelPad.GetAkelDir.
      //Avoid expanding "AkelPad.GetAkel" to "AkelPad.AkelPad.GetAkelDir();".
      for (lpBlockHandle=(BLOCKINFOHANDLE *)lpBlockMaster->firstHandle; lpBlockHandle; lpBlockHandle=lpBlockHandle->next)
      {
        if (lpBlockHandle->lpBlockInfo->nTitleLen > lpBlockInfo->nTitleLen)
        {
          if (xstrstrW(lpBlockHandle->lpBlockInfo->wpTitle, lpBlockHandle->lpBlockInfo->nTitleLen, lpBlockInfo->wpTitle, (int)(nMax - nMin), FALSE, &wpStrBegin, &wpStrEnd))
          {
            ft.dwFlags=0;
            ft.pText=lpBlockHandle->lpBlockInfo->wpTitle;
            ft.dwTextLen=wpStrBegin - lpBlockHandle->lpBlockInfo->wpTitle;

            if (ft.dwTextLen < (UINT_PTR)nMin)
            {
              SendMessage(hWndEdit, AEM_RICHOFFSETTOINDEX, nMin - ft.dwTextLen, (LPARAM)&ciChar);

              if (IsMatch(&ft, &ciChar))
              {
                nMin-=ft.dwTextLen;
                lpBlockInfo=lpBlockHandle->lpBlockInfo;
                break;
              }
            }
          }
        }
      }

      //Smart complete block abbreviations.
      //Avoid expanding "<td" to "<<td".
      if (lpBlockMaster->nBlockLen > lpBlockInfo->nTitleLen)
      {
        if (xstrstrW(lpBlockMaster->wpBlock, lpBlockMaster->nBlockLen, lpBlockInfo->wpTitle, (int)(nMax - nMin), FALSE, &wpStrBegin, &wpStrEnd))
        {
          ft.dwFlags=0;
          ft.pText=lpBlockMaster->wpBlock;
          ft.dwTextLen=wpStrBegin - lpBlockMaster->wpBlock;

          if (ft.dwTextLen <= (UINT_PTR)nMin)
          {
            SendMessage(hWndEdit, AEM_RICHOFFSETTOINDEX, nMin - ft.dwTextLen, (LPARAM)&ciChar);

            if (IsMatch(&ft, &ciChar))
            {
              nMin-=ft.dwTextLen;
            }
          }
        }
      }

      if (lpBlockMaster->nLinesInBlock > 1)
      {
        nMinLine=(int)SendMessage(hWndEdit, EM_EXLINEFROMCHAR, 0, nMin);
        nMinLineIndex=SendMessage(hWndEdit, EM_LINEINDEX, nMinLine, 0);

        if (nMinLineIndex != nMin)
        {
          gtr.cpMin=nMinLineIndex;
          gtr.cpMax=nMin;

          //Get line before word
          if (nLen=(int)SendMessage(hMainWnd, AKD_GETTEXTRANGEW, (WPARAM)hWndEdit, (LPARAM)&gtr))
          {
            wpLine=(wchar_t *)gtr.pText;

            //Get spaces
            for (nSpaceCount=0; wpLine[nSpaceCount] == ' ' || wpLine[nSpaceCount] == '\t'; ++nSpaceCount);
            wpLine[nSpaceCount]='\0';

            if (nSpaceCount)
            {
              nLen=lpBlockMaster->nBlockLen + (lpBlockMaster->nLinesInBlock - 1) * nSpaceCount;

              if (wpIndentBlock=(wchar_t *)GlobalAlloc(GPTR, nLen * sizeof(wchar_t) + 2))
              {
                for (a=0, b=0; b < lpBlockMaster->nBlockLen;)
                {
                  if (lpBlockMaster->wpBlock[b] == '\r')
                  {
                    wpIndentBlock[a++]=lpBlockMaster->wpBlock[b++];

                    for (lpHotSpot=(HOTSPOT *)lpBlockMaster->hHotSpotStack.first; lpHotSpot; lpHotSpot=lpHotSpot->next)
                    {
                      if (lpHotSpot->nHotSpotPos >= a)
                      {
                        lpHotSpot->nHotSpotPos+=nSpaceCount;
                      }
                    }
                    xstrcpyW(wpIndentBlock + a, wpLine);
                    a+=nSpaceCount;
                  }
                  else wpIndentBlock[a++]=lpBlockMaster->wpBlock[b++];
                }
                wpIndentBlock[a]='\0';
              }
            }
            SendMessage(hMainWnd, AKD_FREETEXT, 0, (LPARAM)gtr.pText);
          }
        }
      }
      if (!wpIndentBlock)
        wpIndentBlock=lpBlockMaster->wpBlock;

      cr.cpMin=nMin;
      cr.cpMax=nMax;
      SendMessage(hWndEdit, EM_EXSETSEL64, 0, (LPARAM)&cr);
      SendMessage(hMainWnd, AKD_REPLACESELW, (WPARAM)hWndEdit, (LPARAM)wpIndentBlock);

      if (lpHotSpot=(HOTSPOT *)lpBlockMaster->hHotSpotStack.first)
      {
        cr.cpMin=nMin + lpHotSpot->nHotSpotPos;
        cr.cpMax=nMin + lpHotSpot->nHotSpotPos + lpHotSpot->nHotSpotLen;
        SendMessage(hWndEdit, EM_EXSETSEL64, 0, (LPARAM)&cr);
      }
      if (bAkelEdit)
      {
        lpBlockMaster->nHotSpotBlockBegin=nMin;
        lpCurrentBlockElement=lpBlockMaster;
      }

      if (wpIndentBlock != lpBlockMaster->wpBlock)
        GlobalFree((HGLOBAL)wpIndentBlock);
      bCompletingTitle=FALSE;
    }
    else MessageBeep(MB_OK);
  }
}

BLOCKINFO* StackInsertBlock(STACKBLOCK *hBlockStack)
{
  BLOCKINFO *lpElementNew=NULL;

  if (!StackInsertBefore((stack **)&hBlockStack->first, (stack **)&hBlockStack->last, (stack *)NULL, (stack **)&lpElementNew, sizeof(BLOCKINFO)))
    lpElementNew->nStructType=BIT_BLOCK;
  return lpElementNew;
}

BLOCKINFO* StackInsertAndSortBlock(STACKBLOCK *hBlockStack, wchar_t *wpTitle, int nTitleLen)
{
  BLOCKINFO *lpElement=(BLOCKINFO *)hBlockStack->first;
  BLOCKINFO *lpElementNew=NULL;
  wchar_t wchFirstLowerChar=WideCharLower(*wpTitle);

  if (wchFirstLowerChar < FIRST_NONLATIN)
  {
    if (hBlockStack->lpSorted[wchFirstLowerChar])
      lpElement=(BLOCKINFO *)hBlockStack->lpSorted[wchFirstLowerChar];
    else
      lpElement=(BLOCKINFO *)hBlockStack->first;
  }
  else lpElement=(BLOCKINFO *)hBlockStack->lpSorted[FIRST_NONLATIN];

  while (lpElement)
  {
    if (lpElement->wchFirstLowerChar >= wchFirstLowerChar)
    {
      if (xstrcmpiW(lpElement->wpTitle, wpTitle) >= 0)
        break;
    }
    lpElement=lpElement->next;
  }
  StackInsertBefore((stack **)&hBlockStack->first, (stack **)&hBlockStack->last, (stack *)lpElement, (stack **)&lpElementNew, sizeof(BLOCKINFO));

  if (lpElementNew)
  {
    if (wchFirstLowerChar < FIRST_NONLATIN)
    {
      if (!hBlockStack->lpSorted[wchFirstLowerChar] || (INT_PTR)lpElement == hBlockStack->lpSorted[wchFirstLowerChar])
        hBlockStack->lpSorted[wchFirstLowerChar]=(INT_PTR)lpElementNew;
    }
    else
    {
      if (!hBlockStack->lpSorted[FIRST_NONLATIN] || (INT_PTR)lpElement == hBlockStack->lpSorted[FIRST_NONLATIN])
        hBlockStack->lpSorted[FIRST_NONLATIN]=(INT_PTR)lpElementNew;
    }

    lpElementNew->nStructType=BIT_BLOCK;
    lpElementNew->wchFirstLowerChar=wchFirstLowerChar;
  }
  return lpElementNew;
}

BLOCKINFO* StackGetExactBlock(SYNTAXFILE *lpSyntaxFile, AECHARINDEX *ciCaret, INT_PTR nCaretOffset, INT_PTR *nMin, INT_PTR *nMax)
{
  BLOCKINFO *lpBlockElement;
  AECHARINDEX ciPrevChar;
  AECHARINDEX ciCount;
  int nTitleCount;
  int nChar;

  if (lpSyntaxFile)
  {
    if (!AEC_PrevCharInLineEx(ciCaret, &ciPrevChar))
      return NULL;

    for (lpBlockElement=(BLOCKINFO *)lpSyntaxFile->hExactBlockStack.first; lpBlockElement; lpBlockElement=lpBlockElement->next)
    {
      nTitleCount=lpBlockElement->nTitleLen - 1;
      ciCount=ciPrevChar;

      while ((nChar=AEC_CharAtIndex(&ciCount)) >= 0 && nTitleCount >= 0)
      {
        if (WideCharLower(lpBlockElement->wpTitle[nTitleCount]) != WideCharLower((wchar_t)nChar))
          break;
        --nTitleCount;
        if (!AEC_PrevCharInLine(&ciCount))
          break;
      }
      if (nTitleCount < 0)
      {
        *nMin=nCaretOffset - lpBlockElement->nTitleLen;
        *nMax=nCaretOffset;
        return lpBlockElement;
      }
    }
  }
  return NULL;
}

BLOCKINFO* StackGetBlock(SYNTAXFILE *lpSyntaxFile, HDOCWORDS *hDocWordsStack, const wchar_t *wpTitlePart, int nTitlePartLen, BOOL *bOnlyOne)
{
  BLOCKINFO *lpBlockElement;
  wchar_t wchFirstLowerChar=WideCharLower(*wpTitlePart);

  if (lpSyntaxFile)
  {
    //Blocks
    if (wchFirstLowerChar < FIRST_NONLATIN)
      lpBlockElement=(BLOCKINFO *)lpSyntaxFile->hBlockStack.lpSorted[wchFirstLowerChar];
    else
      lpBlockElement=(BLOCKINFO *)lpSyntaxFile->hBlockStack.lpSorted[FIRST_NONLATIN];

    while (lpBlockElement)
    {
      if (wchFirstLowerChar < FIRST_NONLATIN && wchFirstLowerChar != lpBlockElement->wchFirstLowerChar)
        break;

      if (nTitlePartLen <= lpBlockElement->nTitleLen)
      {
        if (!xstrcmpinW(wpTitlePart, lpBlockElement->wpTitle, nTitlePartLen))
        {
          if (bOnlyOne)
          {
            if (lpBlockElement->next && !xstrcmpinW(wpTitlePart, lpBlockElement->next->wpTitle, nTitlePartLen))
              *bOnlyOne=FALSE;
            else
              *bOnlyOne=TRUE;
          }
          return lpBlockElement;
        }
      }
      lpBlockElement=lpBlockElement->next;
    }

    //HighLight words
    if (bAddHighLightWords)
    {
      WORDINFO *lpWordElement=(WORDINFO *)lpSyntaxFile->hWordStack.first;
      WORDINFO *lpTmp;

      while (lpWordElement)
      {
        if (nTitlePartLen <= lpWordElement->nWordLen)
        {
          if (!xstrcmpinW(wpTitlePart, lpWordElement->wpWord, nTitlePartLen))
          {
            if (bOnlyOne)
            {
              *bOnlyOne=TRUE;

              for (lpTmp=lpWordElement->next; lpTmp; lpTmp=lpTmp->next)
              {
                if (!xstrcmpinW(wpTitlePart, lpTmp->wpWord, nTitlePartLen))
                {
                  if (xstrcmpiW(lpTmp->wpWord, lpWordElement->wpWord) < 0)
                    lpWordElement=lpTmp;
                  *bOnlyOne=FALSE;
                }
                if (!*bOnlyOne && lpWordElement->nWordLen < lpTmp->nWordLen)
                  break;
              }
              if (*bOnlyOne)
              {
                //Skip exact matching word
                //if (lpWordElement->dwFlags & AEHLF_MATCHCASE)
                //{
                //  if (!xstrcmpW(wpTitlePart, lpWordElement->wpWord))
                //    return NULL;
                //}
                //else
                {
                  if (!xstrcmpiW(wpTitlePart, lpWordElement->wpWord))
                    return NULL;
                }
              }
            }
            biHighLightBlock.nTitleLen=(int)xprintfW(wszHighLightTitle, L"%s*", lpWordElement->wpWord);
            biHighLightBlock.wpTitle=wszHighLightTitle;
            biHighLightBlock.wpBlock=lpWordElement->wpWord;
            biHighLightBlock.nBlockLen=lpWordElement->nWordLen;
            biHighLightBlock.nLinesInBlock=1;
            return &biHighLightBlock;
          }
        }
        lpWordElement=lpWordElement->next;
      }
    }
  }

  //Document words
  if (bAddDocumentWords)
  {
    DOCWORDINFO *lpDocWordInfo;

    if (wchFirstLowerChar < FIRST_NONLATIN)
      lpDocWordInfo=(DOCWORDINFO *)hDocWordsStack->lpSorted[wchFirstLowerChar];
    else
      lpDocWordInfo=(DOCWORDINFO *)hDocWordsStack->lpSorted[FIRST_NONLATIN];

    while (lpDocWordInfo)
    {
      if (wchFirstLowerChar < FIRST_NONLATIN && wchFirstLowerChar != lpDocWordInfo->wchFirstLowerChar)
        break;

      if (nTitlePartLen <= lpDocWordInfo->nDocWordLen)
      {
        if (!xstrcmpinW(wpTitlePart, lpDocWordInfo->wpDocWord, nTitlePartLen))
        {
          if (bOnlyOne)
          {
            if (lpDocWordInfo->next && !xstrcmpinW(wpTitlePart, lpDocWordInfo->next->wpDocWord, nTitlePartLen))
              *bOnlyOne=FALSE;
            else
              *bOnlyOne=TRUE;
            if (*bOnlyOne)
            {
              //Skip exact matching word
              if (!xstrcmpiW(wpTitlePart, lpDocWordInfo->wpDocWord))
                return NULL;
            }
          }
          biDocWordBlock.nTitleLen=(int)xprintfW(wszDocWordTitle, L"%s+", lpDocWordInfo->wpDocWord);
          biDocWordBlock.wpTitle=wszDocWordTitle;
          biDocWordBlock.wpBlock=lpDocWordInfo->wpDocWord;
          biDocWordBlock.nBlockLen=lpDocWordInfo->nDocWordLen;
          biDocWordBlock.nLinesInBlock=1;
          return &biDocWordBlock;
        }
      }
      lpDocWordInfo=lpDocWordInfo->next;
    }
  }
  return NULL;
}

void StackFreeBlock(STACKBLOCK *hBlockStack)
{
  BLOCKINFO *lpElement=(BLOCKINFO *)hBlockStack->first;

  while (lpElement)
  {
    GlobalFree((HGLOBAL)lpElement->wpTitle);
    if (!lpElement->master)
    {
      StackClear((stack **)&lpElement->firstHandle, (stack **)&lpElement->lastHandle);
      GlobalFree((HGLOBAL)lpElement->wpBlock);
      StackFreeHotSpot(&lpElement->hHotSpotStack);
    }
    lpElement=lpElement->next;
  }
  StackClear((stack **)&hBlockStack->first, (stack **)&hBlockStack->last);
}

HOTSPOT* StackInsertHotSpot(HSTACK *hStack, int nHotSpotPos)
{
  HOTSPOT *lpElement=NULL;

  if (!StackInsertIndex((stack **)&hStack->first, (stack **)&hStack->last, (stack **)&lpElement, -1, sizeof(HOTSPOT)))
  {
    lpElement->nHotSpotInitPos=nHotSpotPos;
    lpElement->nHotSpotInitLen=0;
    lpElement->nHotSpotPos=nHotSpotPos;
    lpElement->nHotSpotLen=0;
  }
  return lpElement;
}

HOTSPOT* GetHotSpot(BLOCKINFO *lpBlockInfo, HWND hWnd, CHARRANGE64 *cr)
{
  HOTSPOT *lpHotSpot=(HOTSPOT *)lpBlockInfo->hHotSpotStack.first;

  while (lpHotSpot)
  {
    if (lpHotSpot->nHotSpotPos >= 0)
    {
      if (cr->cpMin >= lpBlockInfo->nHotSpotBlockBegin + lpHotSpot->nHotSpotPos &&
          cr->cpMax <= lpBlockInfo->nHotSpotBlockBegin + lpHotSpot->nHotSpotPos + lpHotSpot->nHotSpotLen)
      {
        break;
      }
    }
    lpHotSpot=lpHotSpot->next;
  }
  return lpHotSpot;
}

BOOL NextHotSpot(BLOCKINFO *lpBlockInfo, HWND hWnd, BOOL bPrev)
{
  HOTSPOT *lpHotSpot;
  CHARRANGE64 crOld;
  CHARRANGE64 crNew;

  SendMessage(hWnd, EM_EXGETSEL64, 0, (LPARAM)&crOld);

  if (lpHotSpot=GetHotSpot(lpBlockInfo, hWnd, &crOld))
  {
    if (!bPrev)
    {
      if (lpHotSpot && lpHotSpot->next)
        lpHotSpot=lpHotSpot->next;
      else
        lpHotSpot=(HOTSPOT *)lpBlockInfo->hHotSpotStack.first;
    }
    else
    {
      if (lpHotSpot && lpHotSpot->prev)
        lpHotSpot=lpHotSpot->prev;
      else
        lpHotSpot=(HOTSPOT *)lpBlockInfo->hHotSpotStack.last;
    }

    if (lpHotSpot)
    {
      crNew.cpMin=lpBlockInfo->nHotSpotBlockBegin + lpHotSpot->nHotSpotPos;
      crNew.cpMax=lpBlockInfo->nHotSpotBlockBegin + lpHotSpot->nHotSpotPos + lpHotSpot->nHotSpotLen;
      if (crOld.cpMin != crNew.cpMin || crOld.cpMax != crNew.cpMax)
      {
        SendMessage(hWnd, EM_EXSETSEL64, 0, (LPARAM)&crNew);
        return TRUE;
      }
    }
  }
  return FALSE;
}

void StackResetHotSpot(BLOCKINFO *lpBlockInfo)
{
  HOTSPOT *lpElement=(HOTSPOT *)lpBlockInfo->hHotSpotStack.first;

  while (lpElement)
  {
    lpElement->nHotSpotPos=lpElement->nHotSpotInitPos;
    lpElement->nHotSpotLen=lpElement->nHotSpotInitLen;

    lpElement=lpElement->next;
  }
}

void StackFreeHotSpot(HSTACK *hStack)
{
  StackClear((stack **)&hStack->first, (stack **)&hStack->last);
}

void StackFillDocWord(STACKDELIM *hDelimiterStack, HDOCWORDS *hDocWordsStack, const wchar_t *wpTitlePart, int nTitlePartLen)
{
  DOCWORDINFO *lpDocWordInfo;
  AEFINDTEXTW ft;
  AECHARINDEX ciCaret;
  AECHARINDEX ciCount;
  int nDocWordLen;
  BOOL bFound;

  StackFreeDocWord(hDocWordsStack);

  if (hWndEdit=GetFocusEdit())
  {
    SendMessage(hWndEdit, AEM_GETINDEX, AEGI_FIRSTCHAR, (LPARAM)&ciCount);
    SendMessage(hWndEdit, AEM_GETINDEX, AEGI_CARETCHAR, (LPARAM)&ciCaret);

    ft.dwFlags=0;
    ft.pText=wpTitlePart;
    ft.dwTextLen=nTitlePartLen;
    ft.nNewLine=AELB_R;

    for (;;)
    {
      if (IsMatch(&ft, &ciCount))
      {
        if (IsDelimiterFromLeft(hDelimiterStack, hWndEdit, &ft.crFound.ciMin))
        {
          //Find word end
          ciCount=ft.crFound.ciMax;
          nDocWordLen=(int)ft.dwTextLen;
          lpDocWordInfo=NULL;
          bFound=FALSE;

          do
          {
            if (IsDelimiterFromRight(hDelimiterStack, hWndEdit, &ciCount))
            {
              if (ft.crFound.ciMin.nLine == ft.crFound.ciMax.nLine &&
                  (AEC_IndexCompare(&ciCaret, &ft.crFound.ciMin) < 0 ||
                   AEC_IndexCompare(&ciCaret, &ft.crFound.ciMax) > 0))
              {
                xstrcpynW(wszBuffer, ft.crFound.ciMin.lpLine->wpLine + ft.crFound.ciMin.nCharInLine, nDocWordLen + 1);
                lpDocWordInfo=StackGetDocWord(hDocWordsStack, wszBuffer, nDocWordLen);
                bFound=TRUE;
              }
              break;
            }
            if (++nDocWordLen >= BUFFER_SIZE) break;
          }
          while (AEC_NextCharEx(&ciCount, &ciCount));

          if (bFound && !lpDocWordInfo)
          {
            //Add new word to list
            if (lpDocWordInfo=StackInsertDocWord(hDocWordsStack, wszBuffer, nDocWordLen))
            {
              if (lpDocWordInfo->wpDocWord=(wchar_t *)GlobalAlloc(GPTR, (nDocWordLen + 1) * sizeof(wchar_t)))
                xmemcpy(lpDocWordInfo->wpDocWord, wszBuffer, (nDocWordLen + 1) * sizeof(wchar_t));
              lpDocWordInfo->nDocWordLen=nDocWordLen;
            }
          }
        }
      }

      //Next char
      AEC_IndexInc(&ciCount);
      if (ciCount.nCharInLine >= ciCount.lpLine->nLineLen)
        if (!AEC_NextLine(&ciCount))
          break;
    }
  }
}

DOCWORDINFO* StackInsertDocWord(HDOCWORDS *hStack, wchar_t *wpDocWord, int nDocWordLen)
{
  DOCWORDINFO *lpElement=hStack->first;
  DOCWORDINFO *lpElementNew=NULL;
  wchar_t wchFirstLowerChar=WideCharLower(*wpDocWord);

  if (wchFirstLowerChar < FIRST_NONLATIN)
  {
    if (hStack->lpSorted[wchFirstLowerChar])
      lpElement=(DOCWORDINFO *)hStack->lpSorted[wchFirstLowerChar];
    else
      lpElement=(DOCWORDINFO *)hStack->first;
  }
  else lpElement=(DOCWORDINFO *)hStack->lpSorted[FIRST_NONLATIN];

  while (lpElement)
  {
    if (lpElement->wchFirstLowerChar >= wchFirstLowerChar)
    {
      if (xstrcmpiW(lpElement->wpDocWord, wpDocWord) >= 0)
        break;
    }
    lpElement=lpElement->next;
  }
  StackInsertBefore((stack **)&hStack->first, (stack **)&hStack->last, (stack *)lpElement, (stack **)&lpElementNew, sizeof(DOCWORDINFO));

  if (lpElementNew)
  {
    if (wchFirstLowerChar < FIRST_NONLATIN)
    {
      if (!hStack->lpSorted[wchFirstLowerChar] || (INT_PTR)lpElement == hStack->lpSorted[wchFirstLowerChar])
        hStack->lpSorted[wchFirstLowerChar]=(INT_PTR)lpElementNew;
    }
    else
    {
      if (!hStack->lpSorted[FIRST_NONLATIN] || (INT_PTR)lpElement == hStack->lpSorted[FIRST_NONLATIN])
        hStack->lpSorted[FIRST_NONLATIN]=(INT_PTR)lpElementNew;
    }

    lpElementNew->nStructType=BIT_DOCWORD;
    lpElementNew->wchFirstLowerChar=wchFirstLowerChar;
  }
  return lpElementNew;
}

DOCWORDINFO* StackGetDocWord(HDOCWORDS *hStack, const wchar_t *wpDocWord, int nDocWordLen)
{
  DOCWORDINFO *lpElement;

  for (lpElement=hStack->first; lpElement; lpElement=lpElement->next)
  {
    if (lpElement->nDocWordLen == nDocWordLen)
    {
      if (!xstrcmpiW(wpDocWord, lpElement->wpDocWord))
        return lpElement;
    }
  }
  return NULL;
}

void StackFreeDocWord(HDOCWORDS *hStack)
{
  DOCWORDINFO *lpElement;

  for (lpElement=hStack->first; lpElement; lpElement=lpElement->next)
  {
    GlobalFree((HGLOBAL)lpElement->wpDocWord);
  }
  StackClear((stack **)&hStack->first, (stack **)&hStack->last);
  xmemset(hStack->lpSorted, 0, sizeof(hStack->lpSorted));
}


//// Options

void ReadAutoCompleteOptions(HANDLE hOptions)
{
  WideOption(hOptions, L"AutoCompleteWindowRect", PO_BINARY, (LPBYTE)&rcAutoComplete, sizeof(RECT));
  WideOption(hOptions, L"CompleteWithList", PO_DWORD, (LPBYTE)&dwCompleteWithList, sizeof(DWORD));
  WideOption(hOptions, L"CompleteNext", PO_DWORD, (LPBYTE)&dwCompleteNext, sizeof(DWORD));
  WideOption(hOptions, L"CompletePrev", PO_DWORD, (LPBYTE)&dwCompletePrev, sizeof(DWORD));
  WideOption(hOptions, L"AutoListEnable", PO_DWORD, (LPBYTE)&bAutoListEnable, sizeof(DWORD));
  WideOption(hOptions, L"AutoListAfter", PO_DWORD, (LPBYTE)&nAutoListAfter, sizeof(DWORD));
  WideOption(hOptions, L"AddDocumentWords", PO_DWORD, (LPBYTE)&bAddDocumentWords, sizeof(DWORD));
  WideOption(hOptions, L"CompleteNonSyntaxDocument", PO_DWORD, (LPBYTE)&bCompleteNonSyntaxDocument, sizeof(DWORD));
  WideOption(hOptions, L"MaxDocumentEnable", PO_DWORD, (LPBYTE)&bMaxDocumentEnable, sizeof(DWORD));
  WideOption(hOptions, L"MaxDocumentChars", PO_DWORD, (LPBYTE)&nMaxDocumentChars, sizeof(DWORD));
  WideOption(hOptions, L"AddHighLightWords", PO_DWORD, (LPBYTE)&bAddHighLightWords, sizeof(DWORD));
  WideOption(hOptions, L"RightDelimitersEnable", PO_DWORD, (LPBYTE)&bRightDelimitersEnable, sizeof(DWORD));
  WideOption(hOptions, L"SyntaxDelimitersEnable", PO_DWORD, (LPBYTE)&bSyntaxDelimitersEnable, sizeof(DWORD));
}

void SaveAutoCompleteOptions(HANDLE hOptions, DWORD dwFlags)
{
  if (dwFlags & OF_AUTOCOMPLETE_RECT)
  {
    WideOption(hOptions, L"AutoCompleteWindowRect", PO_BINARY, (LPBYTE)&rcAutoComplete, sizeof(RECT));
  }
  if (dwFlags & OF_AUTOCOMPLETE_SETTINGS)
  {
    WideOption(hOptions, L"CompleteWithList", PO_DWORD, (LPBYTE)&dwCompleteWithList, sizeof(DWORD));
    WideOption(hOptions, L"CompleteNext", PO_DWORD, (LPBYTE)&dwCompleteNext, sizeof(DWORD));
    WideOption(hOptions, L"CompletePrev", PO_DWORD, (LPBYTE)&dwCompletePrev, sizeof(DWORD));
    WideOption(hOptions, L"AutoListEnable", PO_DWORD, (LPBYTE)&bAutoListEnable, sizeof(DWORD));
    WideOption(hOptions, L"AutoListAfter", PO_DWORD, (LPBYTE)&nAutoListAfter, sizeof(DWORD));
    WideOption(hOptions, L"AddDocumentWords", PO_DWORD, (LPBYTE)&bAddDocumentWords, sizeof(DWORD));
    WideOption(hOptions, L"CompleteNonSyntaxDocument", PO_DWORD, (LPBYTE)&bCompleteNonSyntaxDocument, sizeof(DWORD));
    WideOption(hOptions, L"MaxDocumentEnable", PO_DWORD, (LPBYTE)&bMaxDocumentEnable, sizeof(DWORD));
    WideOption(hOptions, L"MaxDocumentChars", PO_DWORD, (LPBYTE)&nMaxDocumentChars, sizeof(DWORD));
    WideOption(hOptions, L"AddHighLightWords", PO_DWORD, (LPBYTE)&bAddHighLightWords, sizeof(DWORD));
    WideOption(hOptions, L"RightDelimitersEnable", PO_DWORD, (LPBYTE)&bRightDelimitersEnable, sizeof(DWORD));
    WideOption(hOptions, L"SyntaxDelimitersEnable", PO_DWORD, (LPBYTE)&bSyntaxDelimitersEnable, sizeof(DWORD));
  }
}

void InitAutoComplete()
{
  bInitAutoComplete=TRUE;

  //Add hotkeys
  {
    PLUGINADDW pa;

    pa.bAutoLoad=FALSE;
    pa.lpParameter=0;

    if (dwCompleteWithList && !SendMessage(hMainWnd, AKD_DLLFINDW, (WPARAM)NULL, dwCompleteWithList))
    {
      pa.pFunction=L"Coder::AutoComplete::WithList";
      pa.wHotkey=(WORD)dwCompleteWithList;
      pa.PluginProc=CompleteWithListProc;
      pfwCompleteWithList=(PLUGINFUNCTION *)SendMessage(hMainWnd, AKD_DLLADDW, 0, (LPARAM)&pa);
    }
    if (dwCompleteNext && !SendMessage(hMainWnd, AKD_DLLFINDW, (WPARAM)NULL, dwCompleteNext))
    {
      pa.pFunction=L"Coder::AutoComplete::Next";
      pa.wHotkey=(WORD)dwCompleteNext;
      pa.PluginProc=CompleteNextProc;
      pfwCompleteNext=(PLUGINFUNCTION *)SendMessage(hMainWnd, AKD_DLLADDW, 0, (LPARAM)&pa);
    }
    if (dwCompletePrev && !SendMessage(hMainWnd, AKD_DLLFINDW, (WPARAM)NULL, dwCompletePrev))
    {
      pa.pFunction=L"Coder::AutoComplete::Prev";
      pa.wHotkey=(WORD)dwCompletePrev;
      pa.PluginProc=CompletePrevProc;
      pfwCompletePrev=(PLUGINFUNCTION *)SendMessage(hMainWnd, AKD_DLLADDW, 0, (LPARAM)&pa);
    }
  }

  //Register AutoComplete class
  {
    WNDCLASSW wndclass;

    wndclass.style        =CS_HREDRAW|CS_VREDRAW;
    wndclass.lpfnWndProc  =AutoCompleteWindowProc;
    wndclass.cbClsExtra   =0;
    wndclass.cbWndExtra   =DLGWINDOWEXTRA;
    wndclass.hInstance    =hInstanceDLL;
    wndclass.hIcon        =NULL;
    wndclass.hCursor      =LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground=(HBRUSH)GetStockObject(HOLLOW_BRUSH);
    wndclass.lpszMenuName =NULL;
    wndclass.lpszClassName=AUTOCOMPLETEW;
    RegisterClassWide(&wndclass);
  }
}

void UninitAutoComplete()
{
  bInitAutoComplete=FALSE;

  //Remove hotkeys
  if (pfwCompleteWithList)
  {
    SendMessage(hMainWnd, AKD_DLLDELETE, 0, (LPARAM)pfwCompleteWithList);
    pfwCompleteWithList=NULL;
  }
  if (pfwCompleteNext)
  {
    SendMessage(hMainWnd, AKD_DLLDELETE, 0, (LPARAM)pfwCompleteNext);
    pfwCompleteNext=NULL;
  }
  if (pfwCompletePrev)
  {
    SendMessage(hMainWnd, AKD_DLLDELETE, 0, (LPARAM)pfwCompletePrev);
    pfwCompletePrev=NULL;
  }
  CloseAutoCompleteWindow();

  UnregisterClassWide(AUTOCOMPLETEW, hInstanceDLL);

  //Save OF_AUTOCOMPLETE_RECT
  if (dwSaveFlags)
  {
    SaveOptions(dwSaveFlags);
    dwSaveFlags=0;
  }
}
