#include "plane-controller-client.h"
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <cinttypes>

#include <binder/Status.h>
#include <binder/ParcelFileDescriptor.h>

#include <com/rdk/hal/planecontrol/GraphicsFbInfo.h>
#include <com/rdk/hal/planecontrol/IPlaneControl.h>

#include "logger.h"

using android::OK;
using android::ProcessState;
using android::String16;
using android::binder::Status;

using com::rdk::hal::planecontrol::GraphicsFbInfo;
using com::rdk::hal::planecontrol::IPlaneControl;

ClientGraphicsFbProviderListener::ClientGraphicsFbProviderListener(
    PlaneControllerClient* client)
    : mClient(client)
{
}

android::binder::Status ClientGraphicsFbProviderListener::onGraphicsFbReleased(
    int32_t oldGraphicsFbId,
    int64_t elapsedRealtimeNanos)
{
    //DEBUG("PlaneControllerClient onGraphicsFbReleased: elapsed time: %" PRId64, elapsedRealtimeNanos);

    if (mClient)
    {
        mClient->notifyGraphicsFbReleased(oldGraphicsFbId, elapsedRealtimeNanos);
    }
    return android::binder::Status::ok();
}

PlaneControllerClient::PlaneControllerClient() : connected(false)
{
}

PlaneControllerClient::~PlaneControllerClient()
{
    close();
}

int PlaneControllerClient::connect(int32_t planeResourceIndex)
{
    android::status_t status;

    status = android::getService(
        String16(IPlaneControl::serviceName().c_str()),
        &mPlaneControl);

    if (status != OK || mPlaneControl == nullptr)
    {
        ERROR("PlaneControllerClient connect: getService failed: %s",
               Status::fromStatusT(status).toString8().c_str());
        return -1;
    }

    connected = true;

    ProcessState::self()->startThreadPool();

    mListener = new ClientGraphicsFbProviderListener(this);

    android::binder::Status binderStatus = mPlaneControl->getGraphicsFbProvider(
        planeResourceIndex,
        mListener,
        &mProvider);

    if (!binderStatus.isOk())
    {
        ERROR("PlaneControllerClient connect: getGraphicsFbProvider failed: %s",
               binderStatus.toString8().c_str());
        return -2;
    }

    if (mProvider == nullptr)
    {
        ERROR("PlaneControllerClient connect: GraphicsFbProvider not available for planeResourceIndex=%d",
               planeResourceIndex);
        return -3;
    }

    binderStatus = mProvider->getCapabilities(&mCapabilities);

    if (!binderStatus.isOk())
    {
        ERROR("PlaneControllerClient connect: getCapabilities failed: %s",
               binderStatus.toString8().c_str());
        return -4;
    }

    INFO("PlaneControllerClient connected success");
    return 0;
}

void PlaneControllerClient::close()
{
    if (!connected)
        return;

    mProvider.clear();
    mListener.clear();
    mPlaneControl.clear();

    {
        std::lock_guard<std::mutex> lock(mMutex);
        std::queue<int32_t> empty;
        std::swap(mReleasedBuffers, empty);
    }
    mCond.notify_all();
}

int PlaneControllerClient::createGraphicsFb(
    int32_t width,
    int32_t height,
    GraphicsFbClientBuffer* outBuffer)
{
    if (!mProvider || !outBuffer)
        return -1;

    GraphicsFbInfo info;
    android::os::ParcelFileDescriptor pfd;

    android::binder::Status status = mProvider->createGraphicsFb(
        width,
        height,
        &info,
        &pfd);

    if (!status.isOk())
    {
        ERROR("PlaneControllerClient createGraphicsFb: failed: %s",
               status.toString8().c_str());
        return -2;
    }

    const int fd = dup(pfd.get());
    
    struct stat st;
    int ret = fstat(fd, &st);

    if (ret == 0) {
        ERROR("PlaneControllerClient createGraphicsFb: FD:%d is valid", fd);
    } else {
        ERROR("PlaneControllerClient createGraphicsFb: FD:%d is INVALID, errno=%s", fd, strerror(errno));
    }
/*
    if (fd < 0)
    {
        ERROR("PlaneControllerClient createGraphicsFb dup graphics fd failed: errno=%s", strerror(errno));
        return -3;
    }
*/
    // Adjust only this mapping block if your generated GraphicsFbInfo field names differ.
    outBuffer->graphicsFbId = info.graphicsFbId;
    outBuffer->width = info.pixelWidth;
    outBuffer->height = info.pixelHeight;
    outBuffer->stride = info.stride;
    outBuffer->offset = info.offset;
    outBuffer->format = GBM_FORMAT_ARGB8888;//TODO vendor specifics in MW
    outBuffer->fd = fd;

    return 0;
}

int PlaneControllerClient::commitGraphicsFb(int32_t graphicsFbId, bool* accepted)
{
    if (!mProvider || !accepted)
        return -1;

    android::binder::Status status = mProvider->commitGraphicsFb(
        graphicsFbId,
        accepted);

    if (!status.isOk())
    {
        ERROR("PlaneControllerClient commitGraphicsFb: failed: %s",
               status.toString8().c_str());
        return -2;
    }

    return 0;
}

int PlaneControllerClient::waitGraphicsFbReleased(int32_t* graphicsFbId)
{
    if (!graphicsFbId)
        return -1;

    std::unique_lock<std::mutex> lock(mMutex);
    mCond.wait(lock, [this] {
        return !mReleasedBuffers.empty();
    });

    *graphicsFbId = mReleasedBuffers.front();
    mReleasedBuffers.pop();
    return 0;
}

bool PlaneControllerClient::destroyBuffer(int32_t graphicsFbId)
{
    android::binder::Status status = mProvider->destroyGraphicsFb(graphicsFbId);

    if (!status.isOk())
    {
        ERROR("PlaneControllerClient destroyBuffer: failed: %s",
               status.toString8().c_str());
        return false;
    }

    return true;
}

void PlaneControllerClient::notifyGraphicsFbReleased(
    int32_t oldGraphicsFbId,
    int64_t elapsedRealtimeNanos)
{
    (void)elapsedRealtimeNanos;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mReleasedBuffers.push(oldGraphicsFbId);
    }
    mCond.notify_one();
}

