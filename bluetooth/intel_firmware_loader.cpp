// X270 AX210 volatile firmware bootstrap using Haiku USB Kit.
// Protocol reference: Linux v6.12 btusb.c and btintel.c (Intel secure send).
// Stop bluetooth_server before use. Firmware remains authenticated by hardware.
#include <USBKit.h>
#include <OS.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <fstream>
#include <iterator>
#include <algorithm>
using Bytes=std::vector<uint8_t>;
static std::mutex lock; static std::condition_variable ready; static std::deque<Bytes> queue;
[[noreturn]] static void fail(const char* why) {fprintf(stderr,"FAIL: %s\n",why);fflush(nullptr);_Exit(1);}
static uint32_t le32(const uint8_t*p){return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static const BUSBEndpoint* receiveEndpoint=nullptr;
template<class P> static Bytes event(P predicate) {
 for(int attempts=0;attempts<32;attempts++) {
  for(auto i=queue.begin();i!=queue.end();++i) if(predicate(*i)) {Bytes out=*i;queue.erase(i);return out;}
  uint8_t buf[1024];auto ep=receiveEndpoint;
  ssize_t n=ep->IsInterrupt()?ep->InterruptTransfer(buf,sizeof(buf)):ep->BulkTransfer(buf,sizeof(buf));
  if(n<=0) fail("USB event read failed");
  for(size_t p=0;p<size_t(n);){if(size_t(n)-p<2||size_t(buf[p+1])+2>size_t(n)-p)fail("fragmented event unsupported");size_t len=buf[p+1]+2;queue.emplace_back(buf+p,buf+p+len);p+=len;}
 }
 fail("matching event not received");
}
static Bytes command(BUSBDevice&dev,const BUSBEndpoint*out,uint16_t op,const Bytes&params={}) {
 if(params.size()>255) fail("command too large");
 Bytes cmd={uint8_t(op),uint8_t(op>>8),uint8_t(params.size())};cmd.insert(cmd.end(),params.begin(),params.end());
 ssize_t sent=op==0xfc09?out->BulkTransfer(cmd.data(),cmd.size()):dev.ControlTransfer(0x20,0,0,0,cmd.size(),cmd.data());
 if(sent!=(ssize_t)cmd.size()) fail("USB command transfer failed");
 if(op==0xfc01) return {}; // Intel reset has no command-complete response.
 auto reply=event([op](const Bytes&e){return e.size()>=6 && e[0]==0x0e && e[3]==uint8_t(op) && e[4]==uint8_t(op>>8);});
 if(reply[5]!=0){fprintf(stderr,"opcode=%04x status=%02x\n",op,reply[5]);fail("controller rejected command");}
 return reply;
}
static void secure(BUSBDevice&dev,const BUSBEndpoint*out,uint8_t type,const uint8_t*data,size_t n) {
 while(n) {size_t count=std::min(n,size_t(252));Bytes p={type};p.insert(p.end(),data,data+count);command(dev,out,0xfc09,p);data+=count;n-=count;}
}
static Bytes file(const char*name){std::ifstream in(name,std::ios::binary);if(!in)fail("cannot open firmware");return Bytes(std::istreambuf_iterator<char>(in),{});}
int main(int argc,char**argv) {
 setbuf(stdout,nullptr);
 if(argc!=4){fprintf(stderr,"Usage: %s USB_PATH ibt-0041-0041.sfi ibt-0041-0041.ddc\n",argv[0]);return 2;}
 std::thread([]{snooze(120000000);fail("overall 120-second deadline expired");}).detach();
 BUSBDevice dev(argv[1]);if(dev.InitCheck()!=B_OK||dev.VendorID()!=0x8087||dev.ProductID()!=0x0032)fail("not the X270 AX210");
 auto config=dev.ActiveConfiguration();if(!config)fail("no active USB configuration");auto iface=config->InterfaceAt(0);
 const BUSBEndpoint *intr=nullptr,*in=nullptr,*out=nullptr;
 for(uint32 i=0;i<iface->CountEndpoints();i++){auto ep=iface->EndpointAt(i);if(ep->IsInterrupt()&&ep->IsInput())intr=ep;if(ep->IsBulk()){if(ep->IsInput())in=ep;else out=ep;}}
 if(!intr||!in||!out)fail("missing HCI endpoints");
 receiveEndpoint=intr;
 auto version=command(dev,out,0xfc05,{0xff});
 uint32_t top=0,radio=0,bt=0;int image=-1,sbe=-1;
 for(size_t p=6;p+2<=version.size();){size_t n=version[p+1];uint8_t type=version[p];p+=2;if(p+n>version.size())fail("truncated TLV");
  if(type==0x10&&n==4)top=le32(&version[p]);if(type==0x11&&n==4)radio=le32(&version[p]);if(type==0x12&&n==4)bt=le32(&version[p]);if(type==0x1c&&n==1)image=version[p];if(type==0x2f&&n==1)sbe=version[p];p+=n;}
 printf("CNVI=%08x CNVR=%08x BT=%08x image=%d SBE=%d\n",top,radio,bt,image,sbe);
 if(top!=0x00400410||radio!=0x00400410||bt!=0x00173700)fail("unsupported hardware revision");
 if(image==3){puts("Operational firmware already running; no download.");fflush(nullptr);_Exit(0);}
 if(image!=1||sbe!=1)fail("unsupported bootloader/security engine");
 Bytes fw=file(argv[2]),ddc=file(argv[3]);
 if(fw.size()<964||fw.size()>4*1024*1024||le32(&fw[8])!=0x10000||fw[644]!=6||le32(&fw[652])!=0x20000)fail("unexpected SFI headers");
 uint32_t boot=0;std::vector<std::pair<size_t,size_t>> fragments;
 size_t start=964,p=start;
 while(p<fw.size()){if(fw.size()-p<3||size_t(fw[p+2])+3>fw.size()-p)fail("truncated SFI command");size_t n=3+fw[p+2];
  if(fw[p]==0x0e&&fw[p+1]==0xfc){if(n<10)fail("short boot parameters");boot=le32(&fw[p+3]);}
  p+=n;if((p-start)%4==0){fragments.push_back({start,p-start});start=p;}}
 if(start!=fw.size()||!boot)fail("unaligned payload or missing boot address");
 for(size_t p=0;p<ddc.size();){size_t n=size_t(ddc[p])+1;if(n<3||n>255||n>ddc.size()-p)fail("malformed DDC");p+=n;}
 printf("Validated %zu-byte SFI; boot=%08x; %zu aligned fragments\n",fw.size(),boot,fragments.size());
 receiveEndpoint=in;
 secure(dev,out,0,&fw[644],128);puts("ECDSA header accepted");secure(dev,out,3,&fw[772],96);secure(dev,out,2,&fw[868],96);
 size_t completed=0;
 for(auto f:fragments){secure(dev,out,1,&fw[f.first],f.second);if(++completed%100==0)printf("payload %zu/%zu\n",completed,fragments.size());}
 auto result=event([](const Bytes&e){return e.size()==7&&e[0]==0xff&&e[2]==6;});
 if(result[3]!=0||result[6]!=0)fail("secure firmware verification failed");
 puts("Signed firmware accepted; starting operational image.");
 receiveEndpoint=intr;
 command(dev,out,0xfc01,{0,1,0,1,uint8_t(boot),uint8_t(boot>>8),uint8_t(boot>>16),uint8_t(boot>>24)});
 event([](const Bytes&e){return e.size()==9&&e[0]==0xff&&e[2]==2;});
 auto standard=command(dev,out,0x1001);printf("Standard HCI version now succeeds (%zu bytes).\n",standard.size());
 for(size_t p=0;p<ddc.size();){size_t n=size_t(ddc[p])+1;command(dev,out,0xfc8b,Bytes(ddc.begin()+p,ddc.begin()+p+n));p+=n;}
 command(dev,out,0x0c03);puts("DDC applied; HCI reset succeeded. Bootstrap complete.");fflush(nullptr);_Exit(0);
}
