// This file was generated by WTL Dialog wizard 
// AddFtpServerDialog.cpp : Implementation of CAddFtpServerDialog

#include "AddFtpServerDialog.h"
#include <Gui/GuiTools.h>
#include <Core/ServerListManager.h>
#include <Func/Settings.h>
#include <Func//IuCommonFunctions.h>

// CAddFtpServerDialog
CAddFtpServerDialog::CAddFtpServerDialog(CUploadEngineList* uploadEngineList)
{
	connectionNameEdited = false;
	downloadUrlEdited = false;
	serverNameEdited = false;
	uploadEngineList_ = uploadEngineList;
}

CAddFtpServerDialog::~CAddFtpServerDialog()
{
}

LRESULT CAddFtpServerDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	SetWindowText(TR("���������� FTP �������"));
	TRC(IDC_CONNECTIONNAMELABEL,"��� ����������");
	TRC(IDC_SERVERSTATIC,"������ [:����]:");
	TRC(IDC_AUTHENTICATIONLABEL,"�����������:");
	TRC(IDC_LOGINLABEL,"�����:");
	TRC(IDC_PASSWORDLABEL,"������:");
	TRC(IDC_REMOTEDIRECTORYLABEL,"��������� ����������:");
	TRC(IDC_DOWNLOADURLLABEL,"URL ��� ����������:");
	TRC(IDCANCEL,"������");
	TRC(IDC_THEURLOFUPLOADEDLABEL,"������ ��� ���������� ����� ��������� ���:");

	//TRC(IDC_CONNECTIONNAMEEDIT, "����� FTP ����������");
	::SetFocus(GetDlgItem(IDC_CONNECTIONNAMEEDIT));
	SetDlgItemText(IDC_REMOTEDIRECTORYEDIT, _T("/"));
	CenterWindow(GetParent());
	return 0;  // Let the system set the focus
}

LRESULT CAddFtpServerDialog::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CString serverName = GuiTools::GetDlgItemText(m_hWnd, IDC_SERVEREDIT);
	serverName.TrimLeft(L" ");
	serverName.TrimRight(L" ");

	if ( serverName.IsEmpty() ) {
		MessageBox(TR("��� ������� �� ����� ���� ������"),TR("������"), MB_ICONERROR);
		return 0;
	}
	CString connectionName = GuiTools::GetDlgItemText(m_hWnd, IDC_CONNECTIONNAMEEDIT);
	connectionName.TrimLeft(L" ");
	connectionName.TrimRight(L" ");
	if ( connectionName.IsEmpty() ) {
		MessageBox(TR("�������� ���������� �� ����� ���� ������"),TR("������"), MB_ICONERROR);
		return 0;
	}

	CString downloadUrl = GuiTools::GetDlgItemText(m_hWnd, IDC_DOWNLOADURLEDIT);
	downloadUrl.TrimLeft(L" ");
	downloadUrl.TrimRight(L" ");
	if ( downloadUrl.IsEmpty() ) {
		MessageBox(TR("������ ��� ���������� �� ����� ���� ������."),TR("������"), MB_ICONERROR);
		return 0;
	}

	CString remoteDirectory = GuiTools::GetDlgItemText(m_hWnd, IDC_REMOTEDIRECTORYEDIT);
	if ( remoteDirectory.Left(1) != _T("/") ) {
		remoteDirectory = _T("/") + remoteDirectory;
	}
	if ( remoteDirectory.Right(1) != _T("/") ) {
		remoteDirectory += _T("/");

	}
	CString login = GuiTools::GetDlgItemText(m_hWnd, IDC_LOGINEDITBOX);
	CString password = GuiTools::GetDlgItemText(m_hWnd, IDC_PASSWORDEDITBOX);

	
	ServerListManager slm(IuCoreUtils::WstringToUtf8((LPCTSTR)(IuCommonFunctions::GetDataFolder()+"Servers\\")), uploadEngineList_, Settings.ServersSettings);
	if ( slm.addFtpServer(IuCoreUtils::WstringToUtf8((LPCTSTR)connectionName), IuCoreUtils::WstringToUtf8((LPCTSTR)serverName), IuCoreUtils::WstringToUtf8((LPCTSTR)login), 
		IuCoreUtils::WstringToUtf8((LPCTSTR)password), IuCoreUtils::WstringToUtf8((LPCTSTR)remoteDirectory), IuCoreUtils::WstringToUtf8((LPCTSTR)downloadUrl)) ) {
			createdServerName_ = IuCoreUtils::Utf8ToWstring(slm.createdServerName()).c_str();
			EndDialog(wID);
	} else {
		CString errorMessage = TR("�� ������� �������� ������.");
		CString reason = IuCoreUtils::Utf8ToWstring(slm.errorMessage()).c_str();
		if ( !reason.IsEmpty() ) {
			errorMessage += CString(L"\r\n") + TR("�������:") + L"\r\n" + reason;
		}
		MessageBox(errorMessage,TR("������"), MB_ICONERROR);

	}

	
	return 0;
}

LRESULT CAddFtpServerDialog::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}


LRESULT CAddFtpServerDialog::OnConnectionNameEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
	if ( GetFocus() == hWndCtl ) {
		connectionNameEdited = !GuiTools::GetDlgItemText(m_hWnd, IDC_CONNECTIONNAMEEDIT).IsEmpty();
	}
	if ( !serverNameEdited) {
		CString connectionName = GuiTools::GetDlgItemText(m_hWnd, IDC_CONNECTIONNAMEEDIT);
		if ( connectionName.Find(_T(".")) != -1 ) {
			SetDlgItemText(IDC_SERVEREDIT, connectionName);
		}
	}

	return 0;
}

LRESULT CAddFtpServerDialog::OnServerEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
	if ( GetFocus() == hWndCtl ) {
		serverNameEdited = !GuiTools::GetDlgItemText(m_hWnd, IDC_SERVEREDIT).IsEmpty();
	}
	if ( !connectionNameEdited ) {
		CString serverName = GuiTools::GetDlgItemText(m_hWnd, IDC_SERVEREDIT);
		SetDlgItemText(IDC_CONNECTIONNAMEEDIT, serverName);
	}
	GenerateDownloadLink();

	return 0;
}

LRESULT CAddFtpServerDialog::OnRemoteDirectoryEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
	GenerateDownloadLink();
	return 0;
}

LRESULT CAddFtpServerDialog::OnDownloadUrlEditChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
	if ( GetFocus() == hWndCtl ) {
		downloadUrlEdited = !GuiTools::GetDlgItemText(m_hWnd, IDC_DOWNLOADURLEDIT).IsEmpty();
		GenerateExampleUrl();
	}
	return 0;
}

CString CAddFtpServerDialog::createdServerName()
{
	return createdServerName_;
}

void CAddFtpServerDialog::GenerateDownloadLink()
{
	if ( !downloadUrlEdited ) {
		CString serverName = GuiTools::GetDlgItemText(m_hWnd, IDC_SERVEREDIT);
		int pos = serverName.ReverseFind(L':');
		if ( pos != -1 ) {
			serverName = serverName.Left(pos);
		}

		CString remoteDirectory = GuiTools::GetDlgItemText(m_hWnd, IDC_REMOTEDIRECTORYEDIT);
		if ( remoteDirectory.Left(1) != _T("/") ) {
			remoteDirectory = _T("/") + remoteDirectory;
		}
		if ( remoteDirectory.Right(1) != _T("/") ) {
			remoteDirectory += _T("/");

		}
		CString generatedDownloadUrl = "http://" + serverName + remoteDirectory;
		
		if ( !serverName.IsEmpty() ) {
			SetDlgItemText(IDC_DOWNLOADURLEDIT, generatedDownloadUrl);
			
		}
	}
	GenerateExampleUrl();
}

void CAddFtpServerDialog::GenerateExampleUrl()
{
	CString downloadUrl = GuiTools::GetDlgItemText(m_hWnd, IDC_DOWNLOADURLEDIT);

	SetDlgItemText(IDC_EXAMPLEURLLABEL, downloadUrl + "example.png");
}
