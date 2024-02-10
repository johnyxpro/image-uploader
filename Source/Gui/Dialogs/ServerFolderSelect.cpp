/*

    Image Uploader -  free application for uploading images/files to the Internet

    Copyright 2007-2018 Sergey Svistunov (zenden2k@gmail.com)

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

 */

#include "ServerFolderSelect.h"

#include "Core/Upload/UploadEngineManager.h"
#include "NewFolderDlg.h"
#include "Core/Upload/AdvancedUploadEngine.h"
#include "Func/WinUtils.h"
#include "Core/Network/NetworkClientFactory.h"
#include "Core/Upload/FolderTask.h"
#include "Core/ServiceLocator.h"
#include "Core/Upload/UploadManager.h"
#include "Core/TaskDispatcher.h"

CServerFolderSelect::CServerFolderSelect(ServerProfile& serverProfile, UploadEngineManager * uploadEngineManager) :serverProfile_(serverProfile)
{
    m_UploadEngine = serverProfile_.uploadEngineData();
    uploadEngineManager_ = uploadEngineManager;
    m_FolderOperationType = FolderOperationType::foGetFolders;
    NetworkClientFactory factory;
    m_NetworkClient = factory.create();
    stopSignal = false;
    isRunning_ = false;
}

CServerFolderSelect::~CServerFolderSelect()
{
}

LRESULT CServerFolderSelect::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    m_FolderTree = GetDlgItem(IDC_FOLDERTREE);
    m_FolderTree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    CenterWindow(GetParent());

    DlgResize_Init();

    HWND hWnd = GetDlgItem(IDC_ANIMATIONSTATIC);
    if (hWnd)
    {
        m_wndAnimation.SubclassWindow(hWnd);
        m_wndAnimation.ShowWindow(SW_HIDE);
    }

    // Internalization
    TRC(IDCANCEL, "Cancel");
    TRC(IDOK, "OK");
    TRC(IDC_NEWFOLDERBUTTON, "Create folder");
    SetWindowText(TR("Folder list"));

    // Get color depth (minimum requirement is 32-bits for alpha blended images).
    //int iBitsPixel = GetDeviceCaps(::GetDC(HWND_DESKTOP), BITSPIXEL);
    int iconWidth = ::GetSystemMetrics(SM_CXSMICON);
    int iconHeight = ::GetSystemMetrics(SM_CYSMICON);
    m_PlaceSelectorImageList.Create(iconWidth, iconHeight, ILC_COLOR32, 0, 6);
    CIcon iconUser;
    iconUser.LoadIconWithScaleDown(MAKEINTRESOURCE(IDI_ICONUSER), iconWidth, iconHeight);
    m_PlaceSelectorImageList.AddIcon(iconUser);
    CIcon iconFolder;
    iconFolder.LoadIconWithScaleDown(MAKEINTRESOURCE(IDI_ICONFOLDER2), iconWidth, iconHeight);
    m_PlaceSelectorImageList.AddIcon(iconFolder);

    CIcon iconSeparator;
    iconSeparator.LoadIconWithScaleDown(MAKEINTRESOURCE(IDI_ICONSEPARATOR), iconWidth, iconHeight);
    m_PlaceSelectorImageList.AddIcon(iconSeparator);

    m_FolderTree.SetImageList(m_PlaceSelectorImageList);
    m_FolderMap[L""] = nullptr;
   
    m_FolderOperationType = FolderOperationType::foGetFolders;
    auto* uploadScript = dynamic_cast<CAdvancedUploadEngine*>(uploadEngineManager_->getUploadEngine(serverProfile_));

    if (!uploadScript)
    {
        SetDlgItemText(IDC_FOLDERLISTLABEL, TR("An error occured while loading script."));
        return 0;
    }
    CString title;
    title.Format(TR("Folder list on server %s for account '%s':"), U2W(m_UploadEngine->Name).GetString(),
                 U2W(serverProfile_.profileName()).GetString());
    SetDlgItemText(IDC_FOLDERLISTLABEL, title);

    uploadScript->setNetworkClient(m_NetworkClient.get());
    uploadScript->getAccessTypeList(m_accessTypeList);

    refreshList();

    m_FolderTree.SetFocus();
    return FALSE;
}

LRESULT CServerFolderSelect::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    HTREEITEM item = m_FolderTree.GetSelectedItem();

    if (!item)
        return 0;
    int nIndex = m_FolderTree.GetItemData(item);

    if (nIndex < 0 || nIndex > m_FolderList.GetCount() - 1)
        return 0;

    if (!::IsWindowEnabled(GetDlgItem(IDOK)))
        return 0;

    m_SelectedFolder = m_FolderList[nIndex];
    EndDialog(wID);

    return 0;
}

LRESULT CServerFolderSelect::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    {
        std::lock_guard<std::mutex> guard(runningScriptMutex_);
        if (isRunning_ && uploadSession_) {
            auto* uploadManager = ServiceLocator::instance()->uploadManager();
            uploadManager->stopSession(uploadSession_.get());
            stopSignal = true;
        } else {
            EndDialog(wID);
        }
    }
    return 0;
}

void CServerFolderSelect::onTaskFinished(UploadTask* task, bool success)
{
    auto* folderTask = dynamic_cast<FolderTask*>(task);
    if (!folderTask) {
        return;
    }
    
    isRunning_ = false;

    if (folderTask->operationType() == FolderOperationType::foGetFolders) {
        m_FolderMap.clear();

        ServiceLocator::instance()->taskRunner()->runInGuiThread([this, success, folderTask]() {
            if (!success) {
                TRC(IDC_FOLDERLISTLABEL, "Failed to load folder list.");
            }
            m_FolderList = folderTask->folderList();
            m_FolderTree.DeleteAllItems();
            BuildFolderTree(m_FolderList.m_folderItems, "");
            m_FolderTree.SelectItem(m_FolderMap[Utf8ToWstring(m_SelectedFolder.id)]);
        });
    } else if (folderTask->operationType() == FolderOperationType::foCreateFolder) {
        m_FolderOperationType = FolderOperationType::foGetFolders;
        m_SelectedFolder = folderTask->folder();
        refreshList();
        return;
    } else if (m_FolderOperationType == FolderOperationType::foModifyFolder) { 
        // Modifying an existing folder
        m_FolderOperationType = FolderOperationType::foModifyFolder;
        m_SelectedFolder = m_newFolder;
        refreshList();
        return;
    }

    OnLoadFinished();
}

void CServerFolderSelect::OnLoadFinished()
{
    stopSignal = false;
    isRunning_ = false;
    ServiceLocator::instance()->taskRunner()->runInGuiThread([this]() {
        BlockWindow(false);
    });
}

void CServerFolderSelect::NewFolder(const CString& parentFolderId)
{
    CFolderItem newFolder;
    CNewFolderDlg dlg(newFolder, true, m_accessTypeList);
    if (dlg.DoModal(m_hWnd) == IDOK)
    {
        m_newFolder = newFolder;
        m_newFolder.parentid = WCstringToUtf8(parentFolderId);
        m_FolderOperationType = FolderOperationType::foCreateFolder;
        if (!isRunning_){
            auto task = std::make_shared<FolderTask>(FolderOperationType::foCreateFolder);
            task->setServerProfile(serverProfile_);
            task->setFolder(m_newFolder);
            using namespace std::placeholders;
            task->addTaskFinishedCallback(std::bind(&CServerFolderSelect::onTaskFinished, this, _1, _2));
            isRunning_ = true;
            UploadManager* uploadManager = ServiceLocator::instance()->uploadManager();
            currentTask_ = task;
            uploadSession_ = std::make_shared<UploadSession>();
            uploadSession_->addTask(task);
            uploadManager->addSession(uploadSession_);
        }
    }
}

LRESULT CServerFolderSelect::OnBnClickedNewfolderbutton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    NewFolder("");
    return 0;
}

LRESULT CServerFolderSelect::OnFolderlistLbnDblclk(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
    HTREEITEM item = m_FolderTree.GetSelectedItem();
    if (item) {
        SendDlgItemMessage(IDOK, BM_CLICK);
    }
    return 0;
}

LRESULT CServerFolderSelect::OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    POINT screenPoint{};
    HWND hwnd = reinterpret_cast<HWND>(wParam);
    if (hwnd != m_FolderTree.m_hWnd) {
        return 0;
    }

    HTREEITEM selectedItem{};
    int xPos = GET_X_LPARAM(lParam);
    int yPos = GET_Y_LPARAM(lParam);

    if (xPos == -1 && yPos == -1) {
        // If the context menu is generated from the keyboard, the application should display
        // the context menu at the location of the current selection rather than at (xPos, yPos).
        CRect rc;
        HTREEITEM firstSelectedItem = m_FolderTree.GetNextSelectedItem(nullptr);
        if (firstSelectedItem) {
            if (m_FolderTree.GetItemRect(firstSelectedItem, &rc, FALSE) ) {
                m_FolderTree.MapWindowPoints(nullptr, &rc);
                screenPoint = { rc.left, rc.bottom };
                selectedItem = firstSelectedItem;
            }
        }
    } else {
        screenPoint.x = xPos;
        screenPoint.y = yPos;
        POINT clientPoint = screenPoint;
        ::ScreenToClient(hwnd, &clientPoint);

        UINT flags{};
        HTREEITEM testItem = m_FolderTree.HitTest(clientPoint, &flags);
        if (testItem && flags & TVHT_ONITEM) {
            selectedItem = testItem;
        }
    }

    if (!selectedItem) {
        return 0;
    }

    m_FolderTree.SelectItem(selectedItem);

    int nIndex = m_FolderTree.GetItemData(selectedItem);
    bool showViewInBrowserItem = false;
    BOOL copyFolderIdFlag = MFS_DISABLED;
    if (nIndex >= 0 && nIndex < m_FolderList.GetCount()) {
        const CFolderItem& folder = m_FolderList[nIndex];
        if (!folder.getViewUrl().empty()) {
            showViewInBrowserItem = true;
        }
        if (!folder.getId().empty() && folder.getId() != CFolderItem::NewFolderMark) {
            copyFolderIdFlag = MFS_ENABLED;
        }
    }

    CMenu sub;
    MENUITEMINFO mi;
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_TYPE | MIIM_ID;
    mi.fType = MFT_STRING;
    sub.CreatePopupMenu();

    mi.wID = ID_EDITFOLDER;
    CString editStr = TR("Edit");
    mi.dwTypeData = const_cast<LPWSTR>(editStr.GetString());
    sub.InsertMenuItem(1, true, &mi);

    if (showViewInBrowserItem) {
        mi.wID = ID_OPENINBROWSER;
        CString openInBrowserStr = TR("Open in Web Browser");
        mi.dwTypeData = const_cast<LPWSTR>(openInBrowserStr.GetString());
        sub.InsertMenuItem(0, true, &mi);
    }

    mi.wID = ID_CREATENESTEDFOLDER;
    CString createNestedFolderStr = TR("Create nested folder");
    mi.dwTypeData = const_cast<LPWSTR>(createNestedFolderStr.GetString());
    sub.InsertMenuItem(2, true, &mi);

    sub.AppendMenu(MFT_STRING | copyFolderIdFlag, ID_COPYFOLDERID, TR("Copy folder's ID"));

    sub.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, screenPoint.x, screenPoint.y, m_hWnd);
    return 0;
}

LRESULT CServerFolderSelect::OnEditFolder(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    HTREEITEM item = m_FolderTree.GetSelectedItem();

    if (!item)
        return 0;
    int nIndex = m_FolderTree.GetItemData(item);

    CFolderItem& folder = m_FolderList[nIndex];

    CNewFolderDlg dlg(folder, false, m_accessTypeList);
    if (dlg.DoModal(m_hWnd) == IDOK)
    {
        m_FolderOperationType = FolderOperationType::foModifyFolder;  // Editing an existing folder
        m_newFolder = folder;
        if (!isRunning_)
        {
            auto task = std::make_shared<FolderTask>(FolderOperationType::foModifyFolder);
            task->setServerProfile(serverProfile_);
            task->setFolder(folder);
            using namespace std::placeholders;
            task->addTaskFinishedCallback(std::bind(&CServerFolderSelect::onTaskFinished, this, _1, _2));
            isRunning_ = true;
            UploadManager* uploadManager = ServiceLocator::instance()->uploadManager();
            currentTask_ = task;
            uploadSession_ = std::make_shared<UploadSession>();
            uploadSession_->addTask(task);
            uploadManager->addSession(uploadSession_);
            //BlockWindow(true);
        }
    }
    return 0;
}

void CServerFolderSelect::BlockWindow(bool Block)
{
    m_wndAnimation.ShowWindow(Block ? SW_SHOW : SW_HIDE);
    ::EnableWindow(GetDlgItem(IDOK), !Block);
    //::EnableWindow(GetDlgItem(IDCANCEL), !Block);
    ::EnableWindow(GetDlgItem(IDC_NEWFOLDERBUTTON), !Block);
    m_FolderTree.EnableWindow(!Block);
    m_FolderTree.SetFocus();
}

LRESULT CServerFolderSelect::OnOpenInBrowser(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    HTREEITEM item = m_FolderTree.GetSelectedItem();

    if (!item)
        return 0;
    int nIndex = m_FolderTree.GetItemData(item);

    CFolderItem& folder = m_FolderList[nIndex];

    CString str = U2W(folder.viewUrl);
    if (!str.IsEmpty())
    {
        WinUtils::ShellOpenFileOrUrl(str, m_hWnd);
    }
    return 0;
}

LRESULT CServerFolderSelect::OnCopyFolderId(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled) {
    HTREEITEM item = m_FolderTree.GetSelectedItem();

    if (!item)
        return 0;
    int nIndex = m_FolderTree.GetItemData(item);

    if (nIndex <0 || nIndex >= m_FolderList.GetCount()) {
        return 0;
    }
    CFolderItem& folder = m_FolderList[nIndex];

    if (!folder.getId().empty() && folder.getId() != CFolderItem::NewFolderMark) {
        CString str = U2W(folder.getId());

        WinUtils::CopyTextToClipboard(str);
    }
    return 0;
}

LRESULT CServerFolderSelect::OnCreateNestedFolder(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    HTREEITEM item = m_FolderTree.GetSelectedItem();

    if (!item)
        return 0;
    int nIndex = m_FolderTree.GetItemData(item);

    CFolderItem& folder = m_FolderList[nIndex];
    NewFolder(folder.id.c_str());
    return 0;
}

void CServerFolderSelect::BuildFolderTree(std::vector<CFolderItem>& list, const CString& parentFolderId)
{
    for (size_t i = 0; i < list.size(); i++)
    {
        CFolderItem& cur = list[i];
        if (cur.parentid.c_str() == parentFolderId)
        {
            CString title = Utf8ToWCstring(cur.title);
            if (cur.itemCount != -1)
                title += _T(" (") + WinUtils::IntToStr(cur.itemCount) + _T(")");
            HTREEITEM res = m_FolderTree.InsertItem(title, 1, 1, m_FolderMap[Utf8ToWstring(cur.parentid)], TVI_SORT  );
            m_FolderTree.SetItemData(res, i);

            m_FolderMap[Utf8ToWstring(cur.id)] = res;
            if (!cur.id.empty()) {
                BuildFolderTree(list, cur.id.c_str());
            }
            if (parentFolderId.IsEmpty()) {
                m_FolderTree.Expand(res);
            }
        }
    }
}


void CServerFolderSelect::refreshList() {
    if (isRunning_) {
        return;
    }
    auto task = std::make_shared<FolderTask>(FolderOperationType::foGetFolders);
    task->setServerProfile(serverProfile_);
    currentTask_ = task;
    using namespace std::placeholders;
    task->addTaskFinishedCallback(std::bind(&CServerFolderSelect::onTaskFinished, this, _1, _2));

    isRunning_ = true;
    uploadSession_ = std::make_shared<UploadSession>();
    uploadSession_->addTask(task);
    BlockWindow(true);
    UploadManager* uploadManager = ServiceLocator::instance()->uploadManager();
    uploadManager->addSession(uploadSession_);
}
