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
 BApplication app("application/x-vnd.local-fn-fullscreen-test");
 BWindow* window = new BWindow(BRect(100,100,499,399), "Fn fullscreen verification", B_TITLED_WINDOW, 0);
 window->Show(); window->Activate(true); snooze(300000);
 BRect before; int32 look;
 window->Lock(); before = window->Frame(); look = window->Look(); window->Unlock();
 Actions* actions = new Actions; actions->Run();
 BMessage action(kAction); action.AddInt32("key",B_F7_KEY); actions->PostMessage(&action);
 snooze(300000);
 window->Lock(); bool entered = window->Look() == B_NO_BORDER_WINDOW_LOOK && window->Frame() == BScreen().Frame(); window->Unlock();
 actions->PostMessage(&action); snooze(300000);
 window->Lock(); bool restored = window->Look() == look && window->Frame() == before; window->Quit();
 if(actions->Lock()) actions->Quit();
 printf("Fullscreen entered: %s; original frame and border restored: %s\n",entered ? "yes" : "no",restored ? "yes" : "no");
 return entered && restored ? 0 : 1;
}
