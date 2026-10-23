// Diagnostic only: return this exact AX210 to bootloader for a full upload test.
#include <USBKit.h>
#include <cstdio>
int main(int argc,char**argv){if(argc!=2)return 2;BUSBDevice dev(argv[1]);if(dev.InitCheck()!=B_OK||dev.VendorID()!=0x8087||dev.ProductID()!=0x0032)return 1;unsigned char command[]={1,0xfc,8,1,1,1,0,0,0,0,0};auto n=dev.ControlTransfer(0x20,0,0,0,sizeof(command),command);printf("Intel hard reset to bootloader: %ld\n",n);return n==sizeof(command)?0:1;}
