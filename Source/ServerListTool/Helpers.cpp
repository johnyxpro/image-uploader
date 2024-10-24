#include "Helpers.h"

#include "Func/WinUtils.h"
#include "3rdpart/GdiplusH.h"
#include "Core/Utils/CoreUtils.h"

namespace ServersListTool::Helpers {

CString MyBytesToString(int64_t nBytes)
{
    return IuCoreUtils::FileSizeToString(nBytes).c_str();
}

CString GetFileInfo(CString fileName, MyFileInfo* mfi)
{
    int64_t fileSize = IuCoreUtils::GetFileSize(W2U(fileName));
    CString result = MyBytesToString(fileSize) + _T("(") + std::to_wstring(fileSize).c_str() + _T(" bytes);");
    CString mimeType = IuCoreUtils::GetFileMimeTypeByContents(W2U(fileName)).c_str();
    result += mimeType + _T(";");
    if (mfi) mfi->mimeType = mimeType;
    if (mimeType.Find(_T("image/")) >= 0) {
        Gdiplus::Image pic(fileName);
        int width = pic.GetWidth();
        int height = pic.GetHeight();
        if (mfi) {
            mfi->width = width;
            mfi->height = height;
        }
        result += WinUtils::IntToStr(width) + _T("x") + WinUtils::IntToStr(height);
    }
    return result;
}

}
