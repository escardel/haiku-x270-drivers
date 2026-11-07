#include <Application.h>
#include <assert.h>
#include "FnShortcuts.cpp"
BInputServerFilter::BInputServerFilter() {}
BInputServerFilter::~BInputServerFilter() {}
status_t BInputServerFilter::InitCheck() { return B_OK; }
filter_result BInputServerFilter::Filter(BMessage*, BList*) { return B_DISPATCH_MESSAGE; }
void BInputServerFilter::_ReservedInputServerFilter1() {}
void BInputServerFilter::_ReservedInputServerFilter2() {}
void BInputServerFilter::_ReservedInputServerFilter3() {}
void BInputServerFilter::_ReservedInputServerFilter4() {}

int main() {
 BApplication app("application/x-vnd.local-fn-popup-test");
 float level, db; bool muted;
 assert(VolumeState(level, db, muted) == B_OK);
 assert(level >= 0 && level <= 1);
 printf("Mixer readback: %.0f%%, %g dB, muted=%d\n",level*100,db,muted);
 ShowLevel(false);
 snooze(300000);
 ShowLevel(true);
 puts("Volume and brightness popups sent; shared ID replaces previous popup, timeout 1.5 seconds.");
}
