#include <Application.h>
#include <MediaRoster.h>
#include <MediaAddOn.h>
#include <cstdio>
#include <cstring>

int main()
{
    BApplication app("application/x-vnd.x270-output-setup");
    BMediaRoster* roster=BMediaRoster::Roster();
    dormant_node_info nodes[16]; int32 count=16;
    status_t status=roster->GetDormantNodes(nodes,&count,nullptr,nullptr,nullptr,
        B_BUFFER_CONSUMER|B_PHYSICAL_OUTPUT);
    printf("GetDormantNodes=%ld count=%ld\n",status,count);
    if(status!=B_OK) return 1;
    for(int32 i=0;i<count;i++) {
        printf("output=%s addon=%ld flavor=%ld\n",nodes[i].name,nodes[i].addon,nodes[i].flavor_id);
        if(strcmp(nodes[i].name,"HD Audio")==0) {
            status=roster->SetAudioOutput(nodes[i]);
            printf("SetAudioOutput=%ld\n",status);
            return status==B_OK?0:2;
        }
    }
    return 3;
}
