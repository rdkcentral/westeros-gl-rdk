#pragma once

#include <stdint.h>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <gbm.h>

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

#include <com/rdk/hal/planecontrol/IPlaneControl.h>
#include <com/rdk/hal/planecontrol/IGraphicsFbProvider.h>
#include <com/rdk/hal/planecontrol/BnGraphicsFbProviderListener.h>
#include <com/rdk/hal/planecontrol/GraphicsFbCapabilities.h>
#include <com/rdk/hal/planecontrol/GraphicsFbInfo.h>

#pragma GCC diagnostic pop

class PlaneControllerClient;

class ClientGraphicsFbProviderListener
    : public com::rdk::hal::planecontrol::BnGraphicsFbProviderListener
{
public:
    explicit ClientGraphicsFbProviderListener(PlaneControllerClient* client);

    android::binder::Status onGraphicsFbReleased(
        int32_t oldGraphicsFbId,
        int64_t elapsedRealtimeNanos) override;

private:
    PlaneControllerClient* mClient;
};

class PlaneControllerClient
{
public:
    struct GraphicsFbClientBuffer
    {
        int32_t graphicsFbId = -1;
        int32_t width = 0;
        int32_t height = 0;
        int32_t stride = 0;
        int32_t offset = 0;
        int32_t format = 0;
        int fd = -1;
    };

    PlaneControllerClient();
    ~PlaneControllerClient();

    int connect(int32_t planeResourceIndex = 0);
    void close();

    int createGraphicsFb(int32_t width, int32_t height, GraphicsFbClientBuffer* outBuffer);
    int commitGraphicsFb(int32_t graphicsFbId, bool* accepted);
    int waitGraphicsFbReleased(int32_t* graphicsFbId);
    bool destroyBuffer(int32_t graphicsFbId);

    void notifyGraphicsFbReleased(int32_t oldGraphicsFbId, int64_t elapsedRealtimeNanos);

private:
    android::sp<com::rdk::hal::planecontrol::IPlaneControl> mPlaneControl;
    android::sp<com::rdk::hal::planecontrol::IGraphicsFbProvider> mProvider;
    android::sp<ClientGraphicsFbProviderListener> mListener;
    com::rdk::hal::planecontrol::GraphicsFbCapabilities mCapabilities;

    std::mutex mMutex;
    std::condition_variable mCond;
    std::queue<int32_t> mReleasedBuffers;
    bool connected;
};

