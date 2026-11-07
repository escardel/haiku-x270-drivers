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
 window->Lock(); bool restored = window->Look() == look && window->Frame() == before; window->Unlock();
 BRect screen = BScreen().Frame(); float split = screen.left + (int32)((screen.Width()+1)/2);
 auto snap = [&](int key, bool right) {
  BMessage action(kAction); action.AddInt32("key",key); actions->PostMessage(&action); snooze(300000);
  window->Lock(); BRect frame = window->Frame(); bool bordered = window->Look() == look; window->Unlock();
  bool horizontal = right ? (frame.left >= split && frame.left < split+20 && frame.right <= screen.right && frame.right > screen.right-20)
   : (frame.left >= screen.left && frame.left < screen.left+20 && frame.right < split && frame.right > split-20);
  return bordered && horizontal && frame.top >= screen.top && frame.top < screen.top+60
   && frame.bottom <= screen.bottom && frame.bottom > screen.bottom-20;
 };
 bool left = snap(kComma,false), right = snap(kPeriod,true);
 actions->PostMessage(&action); snooze(300000);
 bool fromFullscreen = snap(kComma,false);
 BMessage maximize(kAction); maximize.AddInt32("key",kSlash); actions->PostMessage(&maximize); snooze(300000);
 window->Lock(); BRect maximizedFrame = window->Frame(); bool maximized = window->Look() == look
  && maximizedFrame.left >= screen.left && maximizedFrame.left < screen.left+20
  && maximizedFrame.right <= screen.right && maximizedFrame.right > screen.right-20
  && maximizedFrame.top >= screen.top && maximizedFrame.top < screen.top+60
  && maximizedFrame.bottom <= screen.bottom && maximizedFrame.bottom > screen.bottom-20;
 window->Quit();
 if(actions->Lock()) actions->Quit();
 printf("Fullscreen entered: %s; original frame and border restored: %s\n",entered ? "yes" : "no",restored ? "yes" : "no");
 printf("Snap left: %s; snap right: %s; snap out of fullscreen: %s\n",left ? "yes":"no",right ? "yes":"no",fromFullscreen ? "yes":"no");
 printf("Maximize with border preserved: %s\n",maximized ? "yes":"no");
 return entered && restored && left && right && fromFullscreen && maximized ? 0 : 1;
}
