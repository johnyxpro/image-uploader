#include "FileUploadTask.h"

#include <Core/Utils/CoreUtils.h>

FileUploadTask::FileUploadTask(const std::string& fileName, const std::string& displayName, UploadTask* parentTask) : UploadTask(parentTask) {
	fileName_ = fileName;
	originalFileName_ = fileName;
	if ( displayName.empty() ) {
		displayName_ = IuCoreUtils::ExtractFileName(fileName);
	} else {
		displayName_ = displayName;
	}	
}

std::string FileUploadTask::getType() const {
	return "file";
}

std::string FileUploadTask::getMimeType() const {
	return IuCoreUtils::GetFileMimeType(fileName_);
}

int64_t FileUploadTask::getDataLength() const {
	return IuCoreUtils::getFileSize(fileName_);
}

std::string FileUploadTask::getFileName() const {
	return fileName_;
}

void FileUploadTask::setFileName(const std::string& fileName)
{
	fileName_ = fileName;
}

std::string FileUploadTask::getDisplayName() const {
	return displayName_;
}

void FileUploadTask::setDisplayName(const std::string& name)
{
	displayName_ = name;
}

std::string FileUploadTask::originalFileName() const
{
	return originalFileName_;
}