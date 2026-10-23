#include <Application.h>
#include <MediaRecorder.h>
#include <MediaAddOn.h>
#include <MediaRoster.h>
#include <MediaDefs.h>
#include <OS.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

static FILE* sFile;
static int sFrames;
static size_t sBytes;
static int sUniformFrames;
static std::unordered_set<unsigned long long> sVideoHashes;
static unsigned long long sLastHash;
static unsigned sRepeatedFrames;

class ProbeRecorder : public BMediaRecorder {
public:
    using BMediaRecorder::BMediaRecorder;
    using BMediaRecorder::MediaInput;
};

static void receive(void*, bigtime_t, void* data, size_t size,
    const media_format& format)
{
    if (sFrames == 0) {
        if (format.type == B_MEDIA_RAW_VIDEO) {
            const media_raw_video_format& v = format.u.raw_video;
            printf("video %lu x %lu bytes_per_row=%lu colorspace=%lu frame_bytes=%lu\n",
                v.display.line_width, v.display.line_count, v.display.bytes_per_row,
                v.display.format, size);
        } else if (format.type == B_MEDIA_RAW_AUDIO) {
            const media_raw_audio_format& a = format.u.raw_audio;
            printf("audio rate=%.1f channels=%lu format=%lu frame_bytes=%lu\n",
                a.frame_rate, a.channel_count, a.format, size);
        }
    }
    if (format.type == B_MEDIA_RAW_AUDIO || sFrames < 30) {
        fwrite(data, 1, size, sFile);
        sBytes += size;
    }
    if (format.type == B_MEDIA_RAW_VIDEO && size > 0) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        bool uniform = true;
        for (size_t i = 1; i < size; ++i) {
            if (bytes[i] != bytes[0]) { uniform = false; break; }
        }
        sUniformFrames += uniform;
        unsigned long long hash=1469598103934665603ULL;
        for(size_t i=0;i<size;i+=64) { hash^=bytes[i]; hash*=1099511628211ULL; }
        if(sFrames>0 && hash==sLastHash) ++sRepeatedFrames;
        sVideoHashes.insert(hash); sLastHash=hash;
        if(sFrames<3 || sFrames%100==0)
            printf("video_frame=%d hash=%llx unique=%lu repeats=%u\n",sFrames,
                hash,sVideoHashes.size(),sRepeatedFrames);
    }
    ++sFrames;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((argc != 3 && argc != 4)
        || (strcmp(argv[1], "video") && strcmp(argv[1], "audio"))) {
        fprintf(stderr, "usage: %s video|audio output.raw [seconds]\n", argv[0]);
        return 1;
    }
    bool video = !strcmp(argv[1], "video");
    sFile = fopen(argv[2], "wb");
    if (!sFile) { perror("output"); return 2; }
    BApplication app("application/x-vnd.x270-media-capture");
    status_t st;
    BMediaRoster* roster = BMediaRoster::Roster(&st);
    if (!roster || st != B_OK) { printf("roster=%ld\n", st); return 3; }
    {
        dormant_node_info info[32];
        int32 count = 32;
        status_t listStatus = roster->GetDormantNodes(info, &count, NULL, NULL,
            NULL, B_BUFFER_PRODUCER | B_PHYSICAL_INPUT);
        printf("DormantNodes=%ld count=%ld\n", listStatus, count);
        for (int32 i = 0; i < count; ++i)
            printf("dormant[%ld] addon=%ld flavor=%ld name=%s\n", i,
                info[i].addon, info[i].flavor_id, info[i].name);
    }
    media_node node;
    st = video ? roster->GetVideoInput(&node) : roster->GetAudioInput(&node);
    printf("GetInput=%ld node=%ld\n", st, node.node);
    if (st != B_OK) {
        dormant_node_info info[32];
        int32 count = 32;
        status_t listStatus = roster->GetDormantNodes(info, &count, NULL, NULL,
            NULL, B_BUFFER_PRODUCER | B_PHYSICAL_INPUT);
        printf("GetDormantNodes=%ld count=%ld\n", listStatus, count);
        for (int32 i = 0; i < count; ++i) {
            printf("dormant[%ld] addon=%ld flavor=%ld name=%s\n", i,
                info[i].addon, info[i].flavor_id, info[i].name);
            if (st != B_OK && ((video && strstr(info[i].name, "Camera"))
                || (!video && strstr(info[i].name, "HD Audio")))) {
                status_t setStatus = video ? roster->SetVideoInput(info[i])
                    : roster->SetAudioInput(info[i]);
                printf("SetInput=%ld\n", setStatus);
                if (setStatus == B_OK)
                    st = video ? roster->GetVideoInput(&node)
                        : roster->GetAudioInput(&node);
            }
        }
        printf("GetInput retry=%ld node=%ld\n", st, node.node);
    }
    if (st != B_OK) return 4;
    media_output output;
    int32 count = 0;
    st = roster->GetFreeOutputsFor(node, &output, 1, &count,
        video ? B_MEDIA_RAW_VIDEO : B_MEDIA_RAW_AUDIO);
    printf("GetFreeOutputs=%ld count=%ld\n", st, count);
    if (st != B_OK || count < 1) return 5;
    media_format format = output.format;
    format.type = video ? B_MEDIA_RAW_VIDEO : B_MEDIA_RAW_AUDIO;
    ProbeRecorder recorder("X270 probe", format.type);
    st = recorder.InitCheck();
    printf("recorder init=%ld\n", st);
    if (st != B_OK) return 6;
    st = recorder.SetHooks(receive, NULL, NULL);
    printf("SetHooks=%ld\n", st);
    if (st != B_OK) return 7;
    st = recorder.Connect(node, &output, &format);
    printf("Connect=%ld\n", st);
    if (st != B_OK) return 8;
    st = recorder.Start();
    printf("Start=%ld\n", st);
    if (getenv("X270_RECORDER_ONLY") == NULL) {
        media_input recorderInput = recorder.MediaInput();
        status_t consumerStart = roster->StartNode(recorderInput.node, 0);
        printf("Start consumer=%ld node=%ld\n", consumerStart,
            recorderInput.node.node);
        status_t producerStart = roster->StartNode(node, 0);
        printf("Start producer=%ld node=%ld\n", producerStart, node.node);
    }
    if (st == B_OK) snooze((argc == 4 ? atoi(argv[3])
        : video ? 10 : 5) * 1000000LL);
    recorder.Stop();
    recorder.Disconnect();
    roster->ReleaseNode(node);
    fclose(sFile);
    printf("frames=%d uniform_frames=%d bytes=%lu\n", sFrames,
        sUniformFrames, sBytes);
    if(video) printf("unique_video_frames=%lu consecutive_repeats=%u\n",
        sVideoHashes.size(),sRepeatedFrames);
    return sFrames ? 0 : 9;
}
