#include <hmulti_audio.h>
#include <OS.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void put16(FILE* f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE* f, unsigned v) { put16(f, v); put16(f, v >> 16); }

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s output.wav [input_gain_db] [tone]\n", argv[0]);
        return 1;
    }
    float inputGain = argc == 3 ? atof(argv[2]) : 30.0f;
    if (argc == 4) inputGain = atof(argv[2]);
    bool playTone = argc == 4 && strcmp(argv[3], "tone") == 0;
    if (argc == 4 && !playTone) return 1;
    int fd = open("/dev/audio/hmulti/hda/0", O_RDWR);
    if (fd < 0) { perror("open hda"); return 1; }
    multi_description desc = {};
    multi_channel_info channels[32] = {};
    desc.info_size = sizeof(desc);
    desc.request_channel_count = 32;
    desc.channels = channels;
    if (ioctl(fd, B_MULTI_GET_DESCRIPTION, &desc, sizeof(desc)) < 0) {
        perror("description"); return 1;
    }
    printf("device=%s input_channels=%ld input_rates=%lx input_formats=%lx\n",
        desc.friendly_name, desc.input_channel_count, desc.input_rates, desc.input_formats);
    if (desc.input_channel_count < 1 || !(desc.input_formats & B_FMT_16BIT)) return 2;

    multi_mix_control controls[256] = {};
    multi_mix_control_info ci = {};
    ci.info_size = sizeof(ci); ci.control_count = 256; ci.controls = controls;
    if (ioctl(fd, B_MULTI_LIST_MIX_CONTROLS, &ci, sizeof(ci)) >= 0) {
        multi_mix_value values[4] = {};
        values[0].id = 1063; values[0].enable = false;
        values[1].id = 1064; values[1].gain = inputGain;
        values[2].id = 1065; values[2].gain = inputGain;
        values[3].id = 1066; values[3].mux = 5;
        multi_mix_value_info vi = {};
        vi.info_size = sizeof(vi); vi.item_count = 4; vi.values = values;
        if (ioctl(fd, B_MULTI_SET_MIX, &vi, sizeof(vi)) < 0)
            perror("set microphone route and gain");
        for (int i = 0; i < 4; ++i) {
            multi_mix_value v = {}; v.id = values[i].id;
            multi_mix_value_info read = {};
            read.info_size = sizeof(read); read.item_count = 1; read.values = &v;
            if (ioctl(fd, B_MULTI_GET_MIX, &read, sizeof(read)) >= 0)
                printf("control %ld raw=%lx gain=%.1f mux=%lu\n", v.id,
                    *(uint32*)&v.gain, v.gain, v.mux);
        }
    }

    multi_format_info fmt = {};
    fmt.info_size = sizeof(fmt);
    fmt.output.rate = fmt.input.rate = B_SR_48000;
    fmt.output.format = fmt.input.format = B_FMT_16BIT;
    if (ioctl(fd, B_MULTI_SET_GLOBAL_FORMAT, &fmt, sizeof(fmt)) < 0) {
        perror("set format"); return 3;
    }

    buffer_desc play[2][2] = {}, rec[2][2] = {};
    buffer_desc* playPtr[2] = {play[0], play[1]};
    buffer_desc* recPtr[2] = {rec[0], rec[1]};
    multi_buffer_list list = {};
    list.info_size = sizeof(list);
    list.request_playback_buffers = list.request_record_buffers = 2;
    list.request_playback_channels = desc.output_channel_count;
    list.request_record_channels = desc.input_channel_count;
    list.request_playback_buffer_size = list.request_record_buffer_size = 2048;
    list.playback_buffers = playPtr;
    list.record_buffers = recPtr;
    if (ioctl(fd, B_MULTI_GET_BUFFERS, &list, sizeof(list)) < 0) {
        perror("get buffers"); return 4;
    }
    printf("record buffers=%ld channels=%ld frames=%lu\n", list.return_record_buffers,
        list.return_record_channels, list.return_record_buffer_size);
    if (list.return_record_channels < 1 || list.return_record_channels > 2
        || list.return_record_buffers < 1 || list.return_record_buffers > 2) return 5;
    for (int b = 0; b < list.return_playback_buffers; ++b)
        for (int c = 0; c < list.return_playback_channels; ++c)
            for (unsigned i = 0; i < list.return_playback_buffer_size; ++i)
                *(int16*)((char*)play[b][c].base + i * play[b][c].stride) = 0;

    FILE* out = fopen(argv[1], "wb");
    if (!out) { perror("output"); return 6; }
    fputs("RIFF", out); put32(out, 0); fputs("WAVEfmt ", out);
    put32(out, 16); put16(out, 1); put16(out, list.return_record_channels);
    put32(out, 48000); put32(out, 48000 * list.return_record_channels * 2);
    put16(out, list.return_record_channels * 2); put16(out, 16);
    fputs("data", out); put32(out, 0);
    multi_buffer_info info = {};
    info.info_size = sizeof(info);
    unsigned bytes = 0, samples = 0, nonzero = 0;
    double squares = 0;
    bigtime_t start = system_time();
    bigtime_t until = start + (playTone ? 8000000 : 5000000);
    unsigned long long playFrame = 0;
    uint64 lastPlayedFrames = 0, lastRecordedFrames = 0;
    bool havePlayedFrames = false, haveRecordedFrames = false;
    while (system_time() < until) {
        if (ioctl(fd, B_MULTI_BUFFER_EXCHANGE, &info, sizeof(info)) < 0) {
            perror("buffer exchange"); break;
        }
        if (playTone && (!havePlayedFrames
                || info.played_frames_count != lastPlayedFrames)) {
            lastPlayedFrames = info.played_frames_count;
            havePlayedFrames = true;
            int pb = info.playback_buffer_cycle;
            if (pb >= 0 && pb < list.return_playback_buffers) {
                for (unsigned i = 0; i < list.return_playback_buffer_size; ++i) {
                    double seconds = (double)playFrame / 48000.0;
                    int16 tone = seconds >= 3.0 && seconds < 5.0
                        ? (int16)(3000.0 * sin(2.0 * 3.141592653589793
                            * 1000.0 * seconds)) : 0;
                    for (int c = 0; c < list.return_playback_channels; ++c)
                        *(int16*)((char*)play[pb][c].base
                            + i * play[pb][c].stride) = tone;
                    ++playFrame;
                }
            }
        }
        if (haveRecordedFrames
            && info.recorded_frames_count == lastRecordedFrames)
            continue;
        lastRecordedFrames = info.recorded_frames_count;
        haveRecordedFrames = true;
        int b = info.record_buffer_cycle;
        if (b < 0 || b >= list.return_record_buffers) continue;
        for (unsigned i = 0; i < list.return_record_buffer_size; ++i) {
            for (int c = 0; c < list.return_record_channels; ++c) {
                int16 s = *(int16*)((char*)rec[b][c].base + i * rec[b][c].stride);
                put16(out, (uint16)s);
                bytes += 2; ++samples; nonzero += s != 0;
                squares += (double)s * s;
            }
        }
    }
    ioctl(fd, B_MULTI_BUFFER_FORCE_STOP, NULL, 0);
    fseek(out, 4, SEEK_SET); put32(out, bytes + 36);
    fseek(out, 40, SEEK_SET); put32(out, bytes);
    fclose(out); close(fd);
    printf("samples=%u nonzero=%u rms=%.1f bytes=%u\n", samples, nonzero,
        samples ? sqrt(squares / samples) : 0, bytes);
    return bytes ? 0 : 7;
}
