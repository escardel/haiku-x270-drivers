#include <File.h>
#include <Message.h>
#include <TypeConstants.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    const char* path = "/boot/home/config/settings/Media/multi_audio_settings";
    BFile file(path, B_READ_ONLY);
    BMessage root;
    status_t status = root.Unflatten(&file);
    if (status != B_OK) { printf("Unflatten=%ld\n", status); return 1; }
    BMessage audio;
    status = root.FindMessage("HD Audio", &audio);
    if (status != B_OK) { printf("FindMessage=%ld\n", status); return 2; }
    int gainIndex = -1, muxIndex = -1;
    for (int32 i = 0, id; audio.FindInt32("parameterID", i, &id) == B_OK; ++i) {
        const void* data = nullptr;
        ssize_t size = 0;
        audio.FindData("parameterData", B_RAW_TYPE, i, &data, &size);
        if (id == 140) gainIndex = i;
        if (id == 142) muxIndex = i;
        if (id == 140 || id == 142)
            printf("id=%ld index=%ld size=%ld\n", id, i, size);
    }
    if (argc == 1) return 0;
    if (argc != 3 || gainIndex < 0 || muxIndex < 0) return 3;
    float gain[2] = {(float)atof(argv[1]), (float)atof(argv[1])};
    int32 mux = atoi(argv[2]);
    status = audio.ReplaceData("parameterData", B_RAW_TYPE, gainIndex,
        gain, sizeof(gain));
    if (status != B_OK) { printf("gain replace=%ld\n", status); return 4; }
    status = audio.ReplaceData("parameterData", B_RAW_TYPE, muxIndex,
        &mux, sizeof(mux));
    if (status != B_OK) { printf("mux replace=%ld\n", status); return 5; }
    status = root.ReplaceMessage("HD Audio", &audio);
    if (status != B_OK) { printf("root replace=%ld\n", status); return 6; }
    BFile output(path, B_WRITE_ONLY | B_ERASE_FILE);
    status = root.Flatten(&output);
    printf("Flatten=%ld gain=%.1f mux=%ld\n", status, gain[0], mux);
    return status == B_OK ? 0 : 7;
}
