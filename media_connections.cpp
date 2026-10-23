#include <Application.h>
#include <MediaRoster.h>
#include <MediaDefs.h>
#include <cstdio>

int main()
{
    BApplication app("application/x-vnd.x270-media-connections");
    BMediaRoster* roster = BMediaRoster::Roster();
    media_node node;
    status_t status = roster->GetAudioInput(&node);
    printf("GetAudioInput=%ld node=%ld kind=%lx\n", status, node.node,
        node.kind);
    if (status != B_OK)
        return 1;
    media_output outputs[16];
    int32 count = 0;
    status = roster->GetConnectedOutputsFor(node, outputs, 16, &count);
    printf("GetConnectedOutputsFor=%ld count=%ld\n", status, count);
    for (int32 i = 0; i < count; ++i) {
        media_node_id id = roster->NodeIDFor(outputs[i].destination.port);
        media_node other;
        live_node_info info = {};
        if (roster->GetNodeFor(id, &other) == B_OK) {
            roster->GetLiveNodeInfo(other, &info);
            roster->ReleaseNode(other);
        }
        printf("output=%ld name=%s destination=(%ld,%ld) node=%ld consumer=%s\n",
            i, outputs[i].name, outputs[i].destination.port,
            outputs[i].destination.id, id, info.name);
    }
    roster->ReleaseNode(node);
    return 0;
}
