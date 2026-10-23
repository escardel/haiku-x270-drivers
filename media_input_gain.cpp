#include <Application.h>
#include <MediaRoster.h>
#include <ParameterWeb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.x270-input-gain");
    BMediaRoster* roster = BMediaRoster::Roster();
    media_node node;
    status_t status = roster->GetAudioInput(&node);
    if (status != B_OK) { printf("GetAudioInput=%ld\n", status); return 1; }
    BParameterWeb* web = nullptr;
    status = roster->GetParameterWebFor(node, &web);
    if (status != B_OK || web == nullptr) {
        printf("GetParameterWebFor=%ld\n", status); return 2;
    }
    for (int32 i = 0; i < web->CountParameters(); ++i) {
        BParameter* p = web->ParameterAt(i);
        if (p == nullptr || p->Type() == BParameter::B_NULL_PARAMETER) continue;
        unsigned char data[64] = {};
        size_t size = sizeof(data);
        bigtime_t when = 0;
        status = p->GetValue(data, &size, &when);
        printf("id=%ld group=%s name=%s kind=%s type=%d channels=%ld get=%ld size=%lu",
            p->ID(), p->Group() ? p->Group()->Name() : "", p->Name(), p->Kind(),
            p->Type(), p->CountChannels(), status, size);
        if (status == B_OK && p->Type() == BParameter::B_CONTINUOUS_PARAMETER)
            for (size_t j = 0; j + sizeof(float) <= size; j += sizeof(float))
                printf(" value=%.2f", *(float*)(data + j));
        if (status == B_OK && p->Type() == BParameter::B_DISCRETE_PARAMETER && size >= 4)
            printf(" value=%ld", *(int32*)data);
        puts("");
        if (argc == 4 && p->ID() == atoi(argv[1])) {
            if (strcmp(argv[2], "gain") == 0 && p->Type() == BParameter::B_CONTINUOUS_PARAMETER) {
                float values[2] = {(float)atof(argv[3]), (float)atof(argv[3])};
                status = p->SetValue(values, sizeof(float) * p->CountChannels(), system_time());
                printf("set gain status=%ld\n", status);
            } else if (strcmp(argv[2], "mux") == 0 && p->Type() == BParameter::B_DISCRETE_PARAMETER) {
                int32 value = atoi(argv[3]);
                status = p->SetValue(&value, sizeof(value), system_time());
                printf("set mux status=%ld\n", status);
            }
        }
    }
    delete web;
    roster->ReleaseNode(node);
    return 0;
}
