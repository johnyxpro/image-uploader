// This file was generated by WTL Dialog wizard 
// Declaration of the CAddFtpServerDialog

#pragma once
#include "atlheaders.h"
#include "resource.h"       // main symbols
#include "Core/ServerListManager.h"
#include "Gui/Controls/DialogIndirect.h"

class CUploadEngineList;
// CAddFtpServerDialog

class CAddFtpServerDialog : 
    public CCustomDialogIndirectImpl<CAddFtpServerDialog>
{
public:
    CAddFtpServerDialog(CUploadEngineList* uploadEngineList);

    enum { IDD = IDD_ADDFTPSERVERDIALOG };


    BEGIN_MSG_MAP(CAddFtpServerDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
        COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
        COMMAND_HANDLER(IDC_CONNECTIONNAMEEDIT, EN_CHANGE, OnConnectionNameEditChange);
        COMMAND_HANDLER(IDC_SERVEREDIT, EN_CHANGE, OnServerEditChange);
        COMMAND_HANDLER(IDC_REMOTEDIRECTORYEDIT, EN_CHANGE, OnRemoteDirectoryEditChange);
        COMMAND_HANDLER(IDC_DOWNLOADURLEDIT, EN_CHANGE, OnDownloadUrlEditChange);
        COMMAND_HANDLER(IDC_BROWSEPRIVATEKEYBUTTON, BN_CLICKED, OnBnClickedBrowsePrivateKey)
        COMMAND_HANDLER(IDC_SERVERTYPECOMBO, LBN_SELCHANGE, OnServerTypeComboChange)
    END_MSG_MAP()
    // Handler prototypes:
    //  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    //  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    //  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnConnectionNameEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnServerEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnRemoteDirectoryEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnDownloadUrlEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnBnClickedBrowsePrivateKey(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnServerTypeComboChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    CString createdServerName() const;
    CString createdServerLogin() const;
protected:
    void GenerateDownloadLink();
    void GenerateExampleUrl();
    bool connectionNameEdited;
    bool downloadUrlEdited;
    bool serverNameEdited;
    CUploadEngineList* uploadEngineList_;
    CString createdServerName_;
    CString createdServerLogin_;
    CComboBox serverTypeComboBox_;
    ServerListManager::ServerType serverType_;
    void onServerTypeChange();
};


