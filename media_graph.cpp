#include <Application.h>
#include <MediaRoster.h>
#include <TimeSource.h>
#include <cstdio>

static void show(BMediaRoster* roster, const char* label, status_t status,
    const media_node& node)
{
    printf("%s status=%ld node=%ld kind=%lx\n", label, status, node.node, node.kind);
    if (status != B_OK) return;
    media_input inputs[16]; media_output outputs[16]; int32 count = 0;
    status = roster->GetConnectedInputsFor(node, inputs, 16, &count);
    printf(" inputs status=%ld count=%ld\n", status, count);
    for (int32 i=0;i<count;i++)
        printf("  %s source=(%ld,%ld) destination=(%ld,%ld)\n", inputs[i].name,
            inputs[i].source.port, inputs[i].source.id,
            inputs[i].destination.port, inputs[i].destination.id);
    count = 0;
    status = roster->GetConnectedOutputsFor(node, outputs, 16, &count);
    printf(" outputs status=%ld count=%ld\n", status, count);
    for (int32 i=0;i<count;i++)
        printf("  %s source=(%ld,%ld) destination=(%ld,%ld)\n", outputs[i].name,
            outputs[i].source.port, outputs[i].source.id,
            outputs[i].destination.port, outputs[i].destination.id);
    BTimeSource* clock = roster->MakeTimeSourceFor(node);
    if (clock) {
        bigtime_t first = clock->Now(); snooze(100000);
        printf(" clock=%ld running=%d elapsed=%lld\n", clock->ID(),
            clock->IsRunning(), clock->Now()-first);
        clock->Release();
    }
    roster->ReleaseNode(node);
}
int main()
{
    BApplication app("application/x-vnd.x270-media-graph");
    BMediaRoster* roster = BMediaRoster::Roster(); media_node node = {};
    status_t status = roster->GetAudioOutput(&node); show(roster,"AudioOutput",status,node);
    status = roster->GetAudioMixer(&node); show(roster,"AudioMixer",status,node);
    status = roster->GetAudioInput(&node); show(roster,"AudioInput",status,node);
    return 0;
}
