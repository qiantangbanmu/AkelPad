#ifndef _resource_h_
#define _resource_h_
//---------------------------------------------------------------------------
#include "QSearch_defs.h"

#define  IDI_QSEARCH              100

#define  IDD_QSEARCH              1000
#define  IDC_ED_FINDTEXT          1001
#define  IDC_CB_FINDTEXT          1002
#define  IDC_CH_MATCHCASE         1003
#define  IDC_CH_WHOLEWORD         1004
#define  IDC_PB_PROGRESS          1005
#define  IDC_CH_HIGHLIGHTALL      1006
#define  IDC_BT_CANCEL            1010
#define  IDR_MENU_OPTIONS         1020
#define  IDS_EOFREACHED           1040

#define  IDD_QSEARCH_NEW_01       2000
#define  IDC_BT_FINDNEXT          2001
#define  IDC_BT_FINDPREV          2002
#define  IDOK_FINDPREV            2003

#define  IDM_START                10001
#define  IDM_SRCHONTHEFLYMODE     (IDM_START + OPTF_SRCH_ONTHEFLY_MODE)
#define  IDM_SRCHFROMBEGINNING    (IDM_START + OPTF_SRCH_FROM_BEGINNING)
#define  IDM_SRCHUSESPECIALCHARS  (IDM_START + OPTF_SRCH_USE_SPECIALCHARS)
#define  IDM_SRCHUSEREGEXP        (IDM_START + OPTF_SRCH_USE_REGEXP)
#define  IDM_SRCHPICKUPSELECTION  (IDM_START + OPTF_SRCH_PICKUP_SELECTION)
#define  IDM_SRCHSELFINDPICKUP    (IDM_START + OPTF_SRCH_SELFIND_PICKUP)
#define  IDM_SRCHSTOPEOF          (IDM_START + OPTF_SRCH_STOP_EOF)
#define  IDM_SRCHWNDDOCKEDTOP     (IDM_START + OPTF_SRCH_WND_DOCKEDTOP)
#define  IDM_EDITFOCUSSELECTALL   (IDM_START + OPTF_EDIT_FOCUS_SELECTALL)
#define  IDM_CATCHMAINF3          (IDM_START + OPTF_CATCH_MAIN_F3)
#define  IDM_CATCHMAINESC         (IDM_START + OPTF_CATCH_MAIN_ESC)
#define  IDM_HOTKEYHIDESPANEL     (IDM_START + OPTF_HOTKEY_HIDES_PANEL)
#define  IDM_EDITORAUTOFOCUS      (IDM_START + OPTF_EDITOR_AUTOFOCUS)
#define  IDM_QSEARCHAUTOFOCUS     (IDM_START + OPTF_QSEARCH_AUTOFOCUS)

//---------------------------------------------------------------------------
#endif
