#include <Application.h>
#include <SoundPlayer.h>
#include <OS.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <cstdlib>

static std::atomic<unsigned> callbacks(0);
static unsigned long long frames=0;
static void fill(void*, void* buffer, size_t size, const media_raw_audio_format& f)
{
    ++callbacks;
    if (f.format != media_raw_audio_format::B_AUDIO_FLOAT) { memset(buffer,0,size); return; }
    float* samples = static_cast<float*>(buffer);
    size_t count = size / sizeof(float) / f.channel_count;
    for (size_t i=0;i<count;i++,frames++) {
        float v = 0.08f * sin(2 * 3.141592653589793 * 440 * frames / f.frame_rate);
        for (uint32 channel=0;channel<f.channel_count;channel++) *samples++=v;
    }
}
int main(int argc, char** argv)
{
    setvbuf(stdout,nullptr,_IONBF,0);
    BApplication app("application/x-vnd.x270-playback-probe");
    media_raw_audio_format format = media_raw_audio_format::wildcard;
    format.frame_rate=48000; format.channel_count=2;
    format.format=media_raw_audio_format::B_AUDIO_FLOAT;
    format.byte_order=B_MEDIA_HOST_ENDIAN; format.buffer_size=4096;
    BSoundPlayer player(&format,"X270 playback probe",fill,nullptr,nullptr);
    printf("InitCheck=%ld\n",player.InitCheck());
    if(player.InitCheck()!=B_OK) return 1;
    player.SetVolume(1.0f); player.SetHasData(true);
    printf("Start=%ld\n",player.Start());
    int seconds=argc==2?atoi(argv[1]):3;
    if(seconds<1 || seconds>30) seconds=3;
    for(int i=0;i<seconds;i++) { snooze(1000000); printf("second=%d callbacks=%u currentTime=%lld\n",i+1,callbacks.load(),player.CurrentTime()); }
    player.Stop(); printf("stopped callbacks=%u\n",callbacks.load());
    return callbacks.load()>10?0:2;
}
