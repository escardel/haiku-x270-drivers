// Bounded inquiry test. Reports counts only; does not pair or publish nearby IDs.
#include <Application.h>
#include <OS.h>
#include <bluetooth/LocalDevice.h>
#include <bluetooth/DiscoveryAgent.h>
#include <bluetooth/DiscoveryListener.h>
#include <cstdio>
#include <atomic>
#include <cstdlib>
#include <thread>
class Listener : public DiscoveryListener {
public:
 std::atomic<int> count{0}; sem_id done=create_sem(0,"X270 Bluetooth inquiry");
 void DeviceDiscovered(RemoteDevice*,DeviceClass) override {printf("Discovered device #%d\n",++count);}
 void InquiryStarted(status_t status) override {printf("Inquiry started: %ld\n",status);}
 void InquiryResponse(int result) override {printf("Inquiry completed: result=%d devices=%d\n",result,int(count));release_sem(done);}
};
int main(){setbuf(stdout,nullptr);std::thread([]{snooze(45000000);puts("Inquiry watchdog expired");fflush(nullptr);_Exit(124);}).detach();BApplication app("application/x-vnd.x270-bt-probe");printf("Local adapters=%lu\n",LocalDevice::GetLocalDeviceCount());auto device=LocalDevice::GetLocalDevice();if(!device)return 1;uint32 version=0;auto status=device->GetProperty("hci_version",&version);printf("HCI version query=%ld version=%lu\n",status,version);auto listener=new Listener();auto agent=device->GetDiscoveryAgent();if(!agent)return 2;status=agent->StartInquiry(BT_GIAC,listener);printf("StartInquiry=%ld\n",status);if(status!=B_OK)return 3;status=acquire_sem_etc(listener->done,1,B_RELATIVE_TIMEOUT,35000000);printf("Inquiry wait=%ld\n",status);fflush(nullptr);_Exit(status==B_OK?0:4);}
