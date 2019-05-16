#include "UploadTask.h"

#include <boost/format.hpp>

#include "UploadSession.h"
#include "Core/Upload/ScriptUploadEngine.h"
#include "Core/i18n/Translator.h"
#include "Core/Upload/UploadManager.h"

UploadTask::UploadTask()
{
    init();
    parentTask_ = nullptr;
}

UploadTask::UploadTask(UploadTask* parentTask)
{
    init();
    parentTask_ = parentTask;
}

void UploadTask::init()
{
    status_ = StatusInQueue;
    userData_ = nullptr;
    session_ = nullptr;
    role_ = DefaultRole;
    shorteningStarted_ = false;
    stopSignal_ = false;
    currentUploadEngine_ = nullptr;
    tempFileDeleter_ = nullptr;
    uploadSuccess_ = false;
    index_ = 0;
    finishSignalSent_ = false;
    uploadManager_ = nullptr;
}

void UploadTask::childTaskFinished(UploadTask* child)
{
    if (isFinished())
    {
        taskFinished();
    }
}

void UploadTask::taskFinished()
{
    std::lock_guard<std::mutex> lk(finishMutex_);
    if (finishSignalSent_) {
        return;
    }

    if (status_ != StatusStopped) {
        tasksMutex_.lock();
        // std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
        Status newStatus = status_;
        for (size_t i = 0; i < childTasks_.size(); i++) {
            if (childTasks_[i]->type() == TypeFile && childTasks_[i]->status() == StatusFailure) {
                newStatus = StatusFailure;
            }
        }
        tasksMutex_.unlock();
        setStatus(newStatus);
    }
   
    bool success = uploadSuccess();
    
    for (size_t i = 0; i < taskFinishedCallbacks_.size(); i++)
    {
        taskFinishedCallbacks_[i](this, success); // invoke callback
    }
   
    if (session_)
    {
        session_->taskFinished(this);
    }
    
    finishSignalSent_ = true;
}

void UploadTask::statusChanged()
{
    if (OnStatusChanged && status_ != StatusPostponed) // use StatusPostponed to avoid deadlocks
    {
        OnStatusChanged(this);
    }
}

void UploadTask::setCurrentUploadEngine(CAbstractUploadEngine* currentUploadEngine)
{
    currentUploadEngine_ = currentUploadEngine;
}

bool UploadTask::stopSignal() const
{
    return stopSignal_;
}

UploadTask::~UploadTask() {
    delete tempFileDeleter_;
}

UploadTask* UploadTask::parentTask() const
{
    return parentTask_;
}

std::shared_ptr<UploadTask> UploadTask::child(int index)
{
    std::lock_guard<std::recursive_mutex> guard(tasksMutex_);
    return childTasks_[index];
}

bool UploadTask::isRunning()
{
    if (status_ == StatusRunning) {
        return true;
    }
    std::lock_guard<std::recursive_mutex> guard(tasksMutex_);
    for ( auto& it : childTasks_ )
    {
        if (it->isRunning())
        {
            return true;
        }
    }
    return false;
}

bool UploadTask::isRunningItself() const
{
    return status_ == StatusRunning;
}

void UploadTask::setSession(UploadSession* session)
{
    session_ = session;
}

UploadSession* UploadTask::session() const
{
    return session_;
}

bool UploadTask::isFinished()
{
    if (!isFinishedItself()) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
    for (size_t i = 0; i < childTasks_.size(); i++)
    {
        if (!childTasks_[i]->isFinished())
        {
            return false;
        }
    }
    return isFinishedItself();
}

bool UploadTask::isFinishedItself()
{
    return status_ == StatusFinished || status_ == StatusFailure || status_ == StatusStopped || status_ == StatusWaitingChildren;
}

void UploadTask::finishTask(Status status)
{
    setStatus(status);

    if (parentTask_)
    {
        parentTask_->childTaskFinished(this);
    }

    if (isFinished())
    {
        taskFinished();
    } /*else {
        setStatus(StatusWaitingChildren);
    }*/
}

void UploadTask::addChildTask(std::shared_ptr<UploadTask> child)
{
    child->setSession(session_);
    child->setUploadManager(uploadManager_);
    child->parentTask_ = this;
    tasksMutex_.lock();
    childTasks_.push_back(child);
    tasksMutex_.unlock();
    if (session_) {
        session_->childTaskAdded(child.get());
    }
    if (OnChildTaskAdded) {
        OnChildTaskAdded(child.get());
    }
    if (uploadManager_) {
        uploadManager_->insertTaskAfter(this, child);
    }
}

int UploadTask::childCount()
{
    std::lock_guard<std::recursive_mutex> guard(tasksMutex_);
    return childTasks_.size();
}

int UploadTask::index() const {
    return index_;
}

void UploadTask::setIndex(int index) {
    index_ = index;
}

UploadResult* UploadTask::uploadResult()
{
    return &uploadResult_;
}

UploadProgress* UploadTask::progress()
{
    return &progress_;
}

void UploadTask::addTaskFinishedCallback(const TaskFinishedCallback& callback)
{
    auto it = std::find(taskFinishedCallbacks_.begin(), taskFinishedCallbacks_.end(), callback);
    if (it == taskFinishedCallbacks_.end()) {
        taskFinishedCallbacks_.push_back(callback);
    }
}

std::string UploadTask::serverName() const
{
    return serverProfile_.serverName();
}

ServerProfile& UploadTask::serverProfile()
{
    return serverProfile_;
}

void UploadTask::setServerProfile(ServerProfile profile)
{
    serverProfile_ = profile;
}

ServerProfile& UploadTask::urlShorteningServer()
{
    return urlShorteningProfile_;
}

void UploadTask::setUrlShorteningServer(ServerProfile profile)
{
    urlShorteningProfile_ = profile;
}

void UploadTask::setUserData(void* data)
{
    userData_ = data;
}

void* UploadTask::userData() const
{
    return userData_;
}

bool UploadTask::uploadSuccess(bool withChilds)
{
    std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
    int count = childTasks_.size();
    if (!count || !withChilds )
    {
        return uploadSuccess_;
    }
   
    for (int i = 0; i <count; i++)
    {
        if (!childTasks_[i]->isFinished() || !childTasks_[i]->uploadSuccess())
        {
            return false;
        }
    }
    return  uploadSuccess_;
}

void UploadTask::setUploadSuccess(bool success)
{
    uploadSuccess_ = success;
}

UploadTask::Role UploadTask::role() const
{
    return role_;
}

void UploadTask::setRole(Role role)
{
    role_ = role;
}

bool UploadTask::shorteningStarted() const
{
    return shorteningStarted_;
}


void UploadTask::setShorteningStarted(bool started)
{
    shorteningStarted_ = started;
}

void UploadTask::stop()
{
    stopSignal_ = true;
    if (uploadManager_) {
        uploadManager_->removeTaskFromQueue(this);
    }
    if (currentUploadEngine_) {
        currentUploadEngine_->stop();
    }
    
    {
        std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
        for (auto& it : childTasks_) {
            it->stop();
        }
    }
    if (status_ == StatusInQueue || status_ == StatusPostponed) {
        finishTask(StatusStopped);
    }
}

bool UploadTask::isStopped() const
{
    return status_ == StatusStopped;
}

void UploadTask::clearStopFlag() {
    stopSignal_ = false;

    std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
    for (auto& it : childTasks_) {
        it->clearStopFlag();
    }
}

void UploadTask::setStatus(Status status)
{
    if (status_ == status) {
        return;
    }
    status_ = status;

    switch (status) {
        case StatusRunning:
            progress_.statusText = tr("Begin upload");
            break;
        case StatusStopped:
            progress_.statusText = tr("Stopped");
            break;
        case StatusFinished:
            progress_.statusText = tr("Finished");
            break;
        case StatusFailure:
            progress_.statusText = tr("Error");
            break;
        case StatusInQueue:
            progress_.statusText = tr("In queue");
            break;
        case StatusPostponed:
            progress_.statusText = tr("Postponed");
            break;
        case StatusWaitingChildren:
            progress_.statusText = tr("Waiting for child tasks");
            break;
                
        default:
            progress_.statusText = tr("Unknown status");
            
    }
    statusChanged();
}

void UploadTask::setStatusText(const std::string& text)
{
    progress_.statusText = text;
    statusChanged();
}

UploadTask::Status UploadTask::status() const
{
    return status_;
}

void UploadTask::uploadProgress(InfoProgress progress)
{
    progress_.uploaded = progress.Uploaded;
    progress_.totalUpload = progress.Total;
    progress_.isUploading = progress.IsUploading;

    struct timeval tp;
    gettimeofday(&tp, NULL);
    uint64_t curTime = static_cast<uint64_t>(uint64_t(tp.tv_sec) * 1000 + tp.tv_usec / 1000.0);
    if (curTime - progress_.lastUpdateTime > 250 || progress.Uploaded == progress.Total) {
        int64_t Current = progress.Uploaded;

        int speed = 0;
        UploadProgressTimeInfo ti;
        ti.bytes = Current;
        ti.ms = curTime;
        if (progress_.timeBytes.size())
        {
            speed = int(((double)(ti.bytes - progress_.timeBytes[0].bytes) / (curTime - progress_.timeBytes[0].ms) * 1000));
        }

        progress_.timeBytes.push_back(ti);
        if (progress_.timeBytes.size() > 11)
        {
            progress_.timeBytes.pop_front(); //Deleting element at the beginning of the deque
        }
        if (speed > 0)
        {
            progress_.speed = IuCoreUtils::fileSizeToString(speed) + "/s";
        }
        else
        {
            progress_.speed.clear();
        }
        progress_.lastUpdateTime = curTime;
        if (OnUploadProgress)
        {
            OnUploadProgress(this); // invoke upload progress callback
        }
    }
}

/*int UploadTask::getNextTask(UploadTaskAcceptor *acceptor, std::shared_ptr<UploadTask>& outTask)
{
    std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
    int taskCount = childTasks_.size();
    if (!taskCount)
    {
        return 0;
    }
    int count = 0;
    for (auto it = childTasks_.begin(); it != childTasks_.end(); ++it)
    {
        if (it->get()->status()== StatusInQueue )
        {
            count++;
            if (acceptor->canAcceptUploadTask(it->get()))
            {
                outTask = *it;
                return count;
            }
        }
    }
    return count;
}*/

int UploadTask::pendingTasksCount(UploadTaskAcceptor* acceptor)
{
    std::lock_guard<std::recursive_mutex> lock(tasksMutex_);
    int taskCount = childTasks_.size();
    if (!taskCount) {
        return 0;
    }
    int res = 0;
    for (auto it = childTasks_.begin(); it != childTasks_.end(); ++it)
    {
        UploadTask* task = it->get();
        if (!task->isFinished() && !task->isRunning() && acceptor->canAcceptUploadTask(task))
        {
            res++;
        }
    }
    return res;
}

/*std::string UploadTask::UploaderStatusToString(StatusType status, int actionIndex, std::string param)
{
    std::string result;
    switch (status)
    {
    case stWaitingAnswer:
        result = tr("Waiting response from server...");
        break;
    case stCreatingFolder:
        result = tr("Creating folder \"") + param + "\"...";
        break;
    case stUploading:
        result = tr("Sending file to server...");
        break;
    case stAuthorization:
        result = tr("Autorization on server...");
        break;
    case stPerformingAction:
        result = str(boost::format(tr("Performing action #%d..."))% actionIndex);
        break;
    case stUserDescription:
        result = param;
    }
    return result;
};*/


TempFileDeleter* UploadTask::tempFileDeleter(bool create)
{
    if (!tempFileDeleter_ && create)
    {
        tempFileDeleter_ = new TempFileDeleter();
    }
    return tempFileDeleter_;
}

void UploadTask::addTempFile(const std::string& fileName)
{
    tempFileDeleter(true)->addFile(fileName);
}

void UploadTask::deletePostponedChilds() {
    std::lock_guard<std::recursive_mutex> guard(tasksMutex_);
    for ( auto it = childTasks_.begin(); it != childTasks_.end(); ++it ) {
        if ( it->get()->status() == StatusInQueue ) {
            childTasks_.erase(it);
            if (uploadManager_) {
                uploadManager_->removeTaskFromQueue(it->get());
            }
        }
    }
}

bool UploadTask::schedulePostponedChilds() {
    bool res = false;
    std::lock_guard<std::recursive_mutex> guard(tasksMutex_);
    for (auto it = childTasks_.begin(); it != childTasks_.end(); ++it) {
        if (it->get()->status() == StatusPostponed) {
            it->get()->setStatus(StatusInQueue);
            res = true;
        }
    }
    return res;
}

void UploadTask::restartTask(bool fullReset) {
    clearStopFlag();
    finishSignalSent_ = false;
    shorteningStarted_ = false;

    if (fullReset) {
        childTasks_.clear();
        //std::find_if(childTasks_.begin(), childTasks_.end(), [](decltype(childTasks_)::value_type task) {return task->role() == })
        setStatus(StatusInQueue);
    }
}

void UploadTask::setUploadManager(CFileQueueUploader* uploadManager) {
    uploadManager_ = uploadManager;
}