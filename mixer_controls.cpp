#include <hmulti_audio.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    int fd = open("/dev/audio/hmulti/hda/0", O_RDWR);
    if (fd < 0) { perror("open hda"); return 1; }
    multi_mix_control controls[256] = {};
    multi_mix_control_info ci = {};
    ci.info_size = sizeof(ci); ci.control_count = 256; ci.controls = controls;
    if (ioctl(fd, B_MULTI_LIST_MIX_CONTROLS, &ci, sizeof(ci)) < 0) {
        perror("list controls"); return 2;
    }
    if (ci.control_count > 256) return 3;
    if (argc == 4) {
        int id = atoi(argv[1]);
        multi_mix_control* c = NULL;
        for (int i = 0; i < ci.control_count; ++i)
            if (controls[i].id == id) c = &controls[i];
        if (!c) return 4;
        multi_mix_value value = {};
        value.id = id;
        if (!strcmp(argv[2], "gain") && (c->flags & B_MULTI_MIX_GAIN))
            value.gain = atof(argv[3]);
        else if (!strcmp(argv[2], "mute") && (c->flags & B_MULTI_MIX_ENABLE))
            value.enable = atoi(argv[3]) != 0;
        else if (!strcmp(argv[2], "mux") && (c->flags & B_MULTI_MIX_MUX))
            value.mux = atoi(argv[3]);
        else return 5;
        multi_mix_value_info vi = {};
        vi.info_size = sizeof(vi); vi.item_count = 1; vi.values = &value;
        if (ioctl(fd, B_MULTI_SET_MIX, &vi, sizeof(vi)) < 0) {
            perror("set mix"); return 6;
        }
    }
    for (int i = 0; i < ci.control_count; ++i) {
        const multi_mix_control& c = controls[i];
        multi_mix_value v = {}; v.id = c.id;
        multi_mix_value_info vi = {};
        vi.info_size = sizeof(vi); vi.item_count = 1; vi.values = &v;
        bool ok = ioctl(fd, B_MULTI_GET_MIX, &vi, sizeof(vi)) >= 0;
        printf("id=%ld parent=%ld flags=%lx name=%s string=%ld",
            c.id, c.parent, c.flags, c.name, c.string);
        if (c.flags & B_MULTI_MIX_GAIN)
            printf(" gain=%.2f [%.2f,%.2f]", ok ? v.gain : 0,
                c.gain.min_gain, c.gain.max_gain);
        else if (c.flags & B_MULTI_MIX_ENABLE)
            printf(" muted=%d", ok ? v.enable : -1);
        else if (c.flags & B_MULTI_MIX_MUX)
            printf(" mux=%lu", ok ? v.mux : 0);
        puts("");
    }
    close(fd);
}
