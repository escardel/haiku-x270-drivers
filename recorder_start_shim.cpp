#include <MediaRecorder.h>
#include <MediaRoster.h>
#include <dlfcn.h>
#include <cstdio>

using RecorderMethod = status_t (*)(BMediaRecorder*, bool);
using SourceMethod = const media_source& (*)(const BMediaRecorder*);
using HooksMethod = status_t (*)(BMediaRecorder*, BMediaRecorder::ProcessFunc,
    BMediaRecorder::NotifyFunc, void*);
static BMediaRecorder::ProcessFunc originalProcess = nullptr;
static void* originalCookie = nullptr;
static unsigned callbackCount = 0;

static void
process_hook(void*, bigtime_t timestamp, void* data, size_t size,
    const media_format& format)
{
    ++callbackCount;
    if (callbackCount <= 3 || callbackCount % 50 == 0)
        fprintf(stderr, "X270 recorder shim: buffer %u size=%lu first=%02x timestamp=%lld\n",
            callbackCount, size, size ? *(unsigned char*)data : 0, timestamp);
    if (originalProcess)
        originalProcess(originalCookie, timestamp, data, size, format);
}

extern "C" status_t
_ZN14BMediaRecorder8SetHooksEPFvPvlS0_mRK12media_formatEPFvS0_NS_12notificationEzES0_(
    BMediaRecorder* recorder, BMediaRecorder::ProcessFunc process,
    BMediaRecorder::NotifyFunc notify, void* cookie)
{
    HooksMethod original = reinterpret_cast<HooksMethod>(dlsym(RTLD_NEXT,
        "_ZN14BMediaRecorder8SetHooksEPFvPvlS0_mRK12media_formatEPFvS0_NS_12notificationEzES0_"));
    if (!original) return B_ERROR;
    originalProcess = process;
    originalCookie = cookie;
    fprintf(stderr, "X270 recorder shim: SetHooks process=%p\n", (void*)process);
    return original(recorder, process ? process_hook : nullptr, notify, cookie);
}

static status_t
toggle_producer(BMediaRecorder* recorder, bool start)
{
    SourceMethod sourceMethod = reinterpret_cast<SourceMethod>(
        dlsym(RTLD_NEXT, "_ZNK14BMediaRecorder11MediaSourceEv"));
    if (!sourceMethod)
        return B_ERROR;
    const media_source& source = sourceMethod(recorder);
    BMediaRoster* roster = BMediaRoster::Roster();
    if (!roster)
        return B_ERROR;
    media_node_id id = roster->NodeIDFor(source.port);
    media_node node;
    status_t status = roster->GetNodeFor(id, &node);
    if (status != B_OK)
        return status;
    if ((node.kind & B_TIME_SOURCE) != 0)
        status = start ? roster->StartNode(node, 0)
            : roster->StopNode(node, 0, true);
    roster->ReleaseNode(node);
    fprintf(stderr, "X270 recorder shim: %s source node=%ld status=%ld\n",
        start ? "start" : "stop", id, status);
    return status;
}

extern "C" status_t
_ZN14BMediaRecorder5StartEb(BMediaRecorder* recorder, bool force)
{
    RecorderMethod original = reinterpret_cast<RecorderMethod>(
        dlsym(RTLD_NEXT, "_ZN14BMediaRecorder5StartEb"));
    if (!original)
        return B_ERROR;
    status_t status = original(recorder, force);
    if (status == B_OK)
        toggle_producer(recorder, true);
    return status;
}

extern "C" status_t
_ZN14BMediaRecorder4StopEb(BMediaRecorder* recorder, bool force)
{
    fprintf(stderr, "X270 recorder shim: Stop called\n");
    RecorderMethod original = reinterpret_cast<RecorderMethod>(
        dlsym(RTLD_NEXT, "_ZN14BMediaRecorder4StopEb"));
    if (!original)
        return B_ERROR;
    status_t status = original(recorder, force);
    if (status == B_OK)
        toggle_producer(recorder, false);
    return status;
}
