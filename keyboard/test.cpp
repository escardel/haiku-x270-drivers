#include <atomic>
#include <assert.h>
static std::atomic<int> counts[128];
static void RecordAction(int key) { ++counts[key]; }
#define FN_SHORTCUTS_TEST
#include "FnShortcuts.cpp"
// The real implementations are supplied by input_server when loading the add-on.
BInputServerFilter::BInputServerFilter() {}
BInputServerFilter::~BInputServerFilter() {}
status_t BInputServerFilter::InitCheck() { return B_OK; }
filter_result BInputServerFilter::Filter(BMessage*, BList*) { return B_DISPATCH_MESSAGE; }
void BInputServerFilter::_ReservedInputServerFilter1() {}
void BInputServerFilter::_ReservedInputServerFilter2() {}
void BInputServerFilter::_ReservedInputServerFilter3() {}
void BInputServerFilter::_ReservedInputServerFilter4() {}
static filter_result Send(FnFilter& filter, uint32 what, int key, int mods = 0) {
 BMessage message(what); message.AddInt32("key",key); message.AddInt32("modifiers",mods);
 return filter.Filter(&message, NULL);
}
int main() {
 FnFilter filter; assert(filter.InitCheck() == B_OK);
 for (int key = B_F1_KEY; key <= B_F12_KEY; ++key) {
  assert(Send(filter,B_KEY_DOWN,key) == B_SKIP_MESSAGE);
  assert(Send(filter,B_KEY_DOWN,key) == B_SKIP_MESSAGE);
  assert(Send(filter,B_KEY_UP,key) == B_SKIP_MESSAGE);
  assert(Send(filter,B_UNMAPPED_KEY_DOWN,key,B_CONTROL_KEY|B_SHIFT_KEY|B_COMMAND_KEY) == B_SKIP_MESSAGE);
  assert(Send(filter,B_UNMAPPED_KEY_UP,key) == B_SKIP_MESSAGE);
 }
 // Actual keymap: physical left Alt is Left Command; right Alt is Right Option.
 for (int alt : {int(B_LEFT_COMMAND_KEY | B_COMMAND_KEY), int(B_RIGHT_OPTION_KEY | B_OPTION_KEY)}) {
  for (int key = B_F1_KEY; key <= B_F12_KEY; ++key) {
   assert(Send(filter,B_KEY_DOWN,key,alt) == B_DISPATCH_MESSAGE);
   assert(Send(filter,B_KEY_DOWN,key,alt) == B_DISPATCH_MESSAGE);
   // Releasing Alt first still lets the application receive the function-key up.
   assert(Send(filter,B_KEY_UP,key) == B_DISPATCH_MESSAGE);
   assert(Send(filter,B_UNMAPPED_KEY_DOWN,key,alt|B_SHIFT_KEY) == B_DISPATCH_MESSAGE);
   assert(Send(filter,B_UNMAPPED_KEY_UP,key,alt) == B_DISPATCH_MESSAGE);
  }
 }
 // Pressing Alt after a shortcut started must not leak a lone release to apps.
 assert(Send(filter,B_KEY_DOWN,B_F4_KEY) == B_SKIP_MESSAGE);
 assert(Send(filter,B_KEY_DOWN,B_F4_KEY,B_COMMAND_KEY|B_LEFT_COMMAND_KEY) == B_SKIP_MESSAGE);
 assert(Send(filter,B_KEY_UP,B_F4_KEY,B_COMMAND_KEY|B_LEFT_COMMAND_KEY) == B_SKIP_MESSAGE);
 for (int key : {kComma, kPeriod, kSlash}) {
  assert(Send(filter,B_KEY_DOWN,key) == B_DISPATCH_MESSAGE);
  assert(Send(filter,B_KEY_UP,key) == B_DISPATCH_MESSAGE);
  for (int alt : {int(B_LEFT_COMMAND_KEY | B_COMMAND_KEY), int(B_RIGHT_OPTION_KEY | B_OPTION_KEY)}) {
   assert(Send(filter,B_KEY_DOWN,key,alt) == B_SKIP_MESSAGE);
   assert(Send(filter,B_KEY_DOWN,key,alt) == B_SKIP_MESSAGE);
   assert(Send(filter,B_KEY_UP,key) == B_SKIP_MESSAGE);
  }
  // A punctuation press started normally stays normal if Alt is pressed later.
  assert(Send(filter,B_KEY_DOWN,key) == B_DISPATCH_MESSAGE);
  assert(Send(filter,B_KEY_DOWN,key,B_LEFT_COMMAND_KEY|B_COMMAND_KEY) == B_DISPATCH_MESSAGE);
  assert(Send(filter,B_KEY_UP,key,B_LEFT_COMMAND_KEY|B_COMMAND_KEY) == B_DISPATCH_MESSAGE);
 }
 assert(Send(filter,B_KEY_DOWN,0x20) == B_DISPATCH_MESSAGE);
 assert(Send(filter,B_MOUSE_DOWN,B_F1_KEY) == B_DISPATCH_MESSAGE);
 BMessage missing(B_KEY_DOWN); assert(filter.Filter(&missing,NULL) == B_DISPATCH_MESSAGE);
 for (int tries = 0; tries < 100 && counts[kSlash] < 2; ++tries) snooze(10000);
 for (int key = B_F1_KEY; key <= B_F12_KEY; ++key) {
  bool repeat = key == B_F2_KEY || key == B_F3_KEY || key == B_F5_KEY || key == B_F6_KEY;
  assert(counts[key] == (repeat ? 3 : 2) + (key == B_F4_KEY ? 1 : 0));
 }
 assert(counts[kComma] == 2 && counts[kPeriod] == 2 && counts[kSlash] == 2);
 puts("PASS: Alt+comma/period snapping and Alt+slash maximize; shortcuts and repeat policy; left/right Alt bypass without shortcut actions; coherent releases when Alt changes mid-press; unrelated input preserved.");
}
