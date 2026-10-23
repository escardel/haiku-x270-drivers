// Read-only HCI version probes through Haiku USB Kit. Stop bluetooth_server first.
#include <USBKit.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
int main(int argc,char**argv) {
 setbuf(stdout,nullptr);
 BUSBDevice dev(argc>1?argv[1]:"/dev/bus/usb/0/6");
 if(dev.InitCheck()!=B_OK || dev.VendorID()!=0x8087 || dev.ProductID()!=0x0032) return 1;
 auto config=dev.ActiveConfiguration(); if(!config) return 2;
 auto iface=config->InterfaceAt(0); const BUSBEndpoint* events=nullptr;
 for(uint32 i=0;i<iface->CountEndpoints();i++) {auto ep=iface->EndpointAt(i); if(ep->IsInterrupt()&&ep->IsInput()) events=ep;}
 if(!events) return 3;
 // Intel Read Version (TLV request 0xff), then standard Read Local Version.
 uint8_t commands[][4]={{0x05,0xfc,0x01,0xff},{0x01,0x10,0,0}};
 for(auto &cmd:commands) {
  auto sent=dev.ControlTransfer(0x20,0,0,0,3+cmd[2],cmd);
  printf("command %02x%02x send=%ld\n",cmd[1],cmd[0],(long)sent);
  if(sent<0) return 4;
  uint8_t buf[260]={}; auto got=events->InterruptTransfer(buf,sizeof(buf));
  printf("event bytes=%ld:",(long)got);for(int i=0;i<got;i++) printf(" %02x",buf[i]);puts("");
  if(got<0) return 5;
 }
}
