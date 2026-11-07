#include <InputServerFilter.h>
#include <FindDirectory.h>
#include <Path.h>
#include <InterfaceDefs.h>
#include <List.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <Roster.h>
#include <Screen.h>
#include <Window.h>
#include <WindowInfo.h>
#include <MessengerPrivate.h>
#include <Notification.h>
#include <MediaRoster.h>
#include <ParameterWeb.h>
#include <vector>
#include <algorithm>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32 kAction = 'fnac';
static const int32 kComma = 0x53, kPeriod = 0x54;
static void Log(const char* text, status_t status = B_OK)
{
 BPath path;
 if (find_directory(B_USER_LOG_DIRECTORY, &path, true) != B_OK) return;
 path.Append("fn-shortcuts.log");
 FILE* f = fopen(path.Path(), "a");
 if (f) { fprintf(f, "%lld %s: %ld\n", (long long)system_time(), text, (long)status); fclose(f); }
}
// Query the mixer after changing it: setvolume's toggle output reports the old state.
static status_t VolumeState(float& level, float& db, bool& muted)
{
 BMediaRoster* roster = BMediaRoster::Roster();
 if (!roster) return B_ERROR;
 media_node mixer;
 status_t status = roster->GetAudioMixer(&mixer);
 if (status != B_OK) return status;
 BParameterWeb* web = NULL;
 status = roster->GetParameterWebFor(mixer, &web);
 roster->ReleaseNode(mixer);
 if (status != B_OK) return status;
 BParameter* mute = NULL;
 status = B_NAME_NOT_FOUND;
 for (int32 i = 0; i < web->CountParameters(); ++i) {
  BParameter* parameter = web->ParameterAt(i);
  if (!strcmp(parameter->Kind(), B_MUTE)) mute = parameter;
  if (strcmp(parameter->Kind(), B_MASTER_GAIN)) continue;
  BContinuousParameter* gain = dynamic_cast<BContinuousParameter*>(parameter);
  if (!gain || !mute) break;
  std::vector<float> values(std::max(1L, (long)gain->CountChannels()));
  size_t size = values.size() * sizeof(float); bigtime_t when;
  status = gain->GetValue(values.data(), &size, &when);
  if (status != B_OK) break;
  db = values[0];
  float range = gain->MaxValue() - gain->MinValue();
  level = range > 0 ? (db - gain->MinValue()) / range : 0;
  int32 value = 0; size = sizeof(value);
  status = mute->GetValue(&value, &size, &when);
  muted = value != 0;
  break;
 }
 delete web;
 return status;
}
static void ShowLevel(bool brightness, bool failed = false)
{
 float level = 0, db = 0; bool muted = false;
 status_t status = brightness ? BScreen().GetBrightness(&level) : VolumeState(level, db, muted);
 char text[128];
 if (failed || status != B_OK) {
  snprintf(text, sizeof(text), "%s unavailable", brightness ? "Brightness adjustment" : "Volume adjustment");
 } else if (brightness) {
  snprintf(text, sizeof(text), "%d%%", (int)(level * 100 + 0.5f));
 } else if (muted) {
  snprintf(text, sizeof(text), "Muted");
 } else {
  snprintf(text, sizeof(text), "%d%% (%g dB)", (int)(level * 100 + 0.5f), db);
 }
 BNotification popup(failed || status != B_OK ? B_ERROR_NOTIFICATION : B_PROGRESS_NOTIFICATION);
 popup.SetGroup("Keyboard shortcuts");
 popup.SetMessageID("fn-system-level");
 popup.SetTitle(brightness ? "Brightness" : "Volume");
 popup.SetContent(text);
 popup.SetProgress(muted ? 0 : std::max(0.0f, std::min(1.0f, level)));
 Log("level popup", popup.Send(1500000));
}
struct SavedWindow { BMessenger target; BRect frame; int32 look; };
static status_t Get(BMessenger target, const char* name, BMessage& reply)
{
 BMessage request(B_GET_PROPERTY); request.AddSpecifier(name);
 return target.SendMessage(&request, &reply, 200000, 200000);
}
static status_t Set(BMessenger target, const char* name, const BRect* frame, int32 look = 0)
{
 BMessage request(B_SET_PROPERTY), reply; request.AddSpecifier(name);
 if (frame) request.AddRect("data", *frame); else request.AddInt32("data", look);
 status_t status = target.SendMessage(&request, &reply, 200000, 200000);
 int32 error;
 if (status == B_OK && reply.FindInt32("error", &error) == B_OK) status = error;
 return status;
}
class Actions : public BLooper {
 std::map<int32, SavedWindow> fSaved;
 void Snap(bool right)
 {
  app_info app;
  if (be_roster->GetActiveAppInfo(&app) != B_OK) return;
  int32* tokens = NULL; int32 count = 0;
  if (BPrivate::get_window_order(current_workspace(), &tokens, &count) != B_OK) return;
  for (int32 i = 0; i < count; ++i) {
   client_window_info* info = get_window_info(tokens[i]);
   if (!info) continue;
   if (info->team != app.team || info->is_mini || info->show_hide_level != 0
       || info->feel != B_NORMAL_WINDOW_FEEL) { free(info); continue; }
   BMessenger target;
   BMessenger::Private(target).SetTo(info->team, info->client_port, info->client_token);
   int32 token = info->server_token;
   auto saved = fSaved.find(token);
   if (saved != fSaved.end() && saved->second.target == target) {
    status_t status = Set(target, "Look", NULL, saved->second.look);
    if (status != B_OK) { Log("snap restore border", status); free(info); break; }
    fSaved.erase(saved);
    free(info); info = get_window_info(token);
    if (!info) break;
   }
   BScreen screen;
   if (!screen.IsValid()) { free(info); break; }
   BRect frame = screen.Frame();
   float split = frame.left + (int32)((frame.Width() + 1) / 2);
   if (right) frame.left = split; else frame.right = split - 1;
   // Fit the entire decorated window within its screen half.
   frame.left += info->border_size; frame.right -= info->border_size;
   frame.top += info->tab_height + info->border_size;
   frame.bottom -= info->border_size;
   free(info);
   Log(right ? "snap right" : "snap left", Set(target, "Frame", &frame));
   break;
  }
  free(tokens);
 }
 void Fullscreen()
 {
  app_info app;
  if (be_roster->GetActiveAppInfo(&app) != B_OK) return;
  if (!strcmp(app.signature, "application/x-vnd.Be-TRAK") || !strcmp(app.signature, "application/x-vnd.Be-TSKB")) return;
  int32* tokens = NULL; int32 count = 0;
  if (BPrivate::get_window_order(current_workspace(), &tokens, &count) != B_OK) return;
  for (int32 i = 0; i < count; ++i) {
   client_window_info* info = get_window_info(tokens[i]);
   if (!info) continue;
   bool eligible = info->team == app.team && !info->is_mini && info->show_hide_level == 0 && info->feel == B_NORMAL_WINDOW_FEEL;
   if (!eligible) { free(info); continue; }
   BMessenger target;
   BMessenger::Private(target).SetTo(info->team, info->client_port, info->client_token);
   int32 token = info->server_token; free(info);
   auto saved = fSaved.find(token);
   if (saved != fSaved.end() && saved->second.target == target) {
    status_t status = Set(target, "Look", NULL, saved->second.look);
    if (status == B_OK) status = Set(target, "Frame", &saved->second.frame);
    if (status == B_OK) fSaved.erase(saved);
    Log("restore fullscreen window", status);
   } else {
    BMessage frameReply, lookReply; SavedWindow state; state.target = target;
    if (Get(target, "Frame", frameReply) != B_OK || frameReply.FindRect("result", &state.frame) != B_OK
      || Get(target, "Look", lookReply) != B_OK || lookReply.FindInt32("result", &state.look) != B_OK) break;
    BScreen screen; if (!screen.IsValid()) break;
    BRect frame = screen.Frame();
    status_t status = Set(target, "Look", NULL, B_NO_BORDER_WINDOW_LOOK);
    if (status == B_OK) {
     fSaved[token] = state; status = Set(target, "Frame", &frame);
     if (status != B_OK) { Set(target, "Look", NULL, state.look); fSaved.erase(token); }
    }
    Log("fullscreen window", status);
   }
   break;
  }
  free(tokens);
 }
public:
 Actions() : BLooper("Fn shortcut actions") {}
 void MessageReceived(BMessage* message)
 {
  if (message->what != kAction) { BLooper::MessageReceived(message); return; }
  int32 key; if (message->FindInt32("key", &key) != B_OK) return;
#ifdef FN_SHORTCUTS_TEST
  RecordAction(key);
  return;
#endif
  const char* command = NULL;
  switch (key) {
   case B_F1_KEY: command = "/bin/setvolume -t"; break;
   case B_F2_KEY: command = "/bin/setvolume -d 3"; break;
   case B_F3_KEY: command = "/bin/setvolume -i 3"; break;
   case B_F5_KEY: command = "/bin/screenmode -b -0.05"; break;
   case B_F6_KEY: command = "/bin/screenmode -b +0.05"; break;
   case B_F7_KEY: Fullscreen(); return;
   case kComma: Snap(false); return;
   case kPeriod: Snap(true); return;
   case B_F9_KEY: command = "/bin/open /boot/system/preferences"; break;
   case B_F11_KEY: command = "/bin/open /boot/system/apps/Terminal"; break;
   case B_F8_KEY: command = "/bin/open /boot/system/apps/Firefox/Firefox"; break;
   case B_F12_KEY: command = "/bin/open /boot/system/apps/WindowTailor"; break;
  }
  if (command) {
   int result = system(command);
   Log(command, result);
   if (key == B_F1_KEY || key == B_F2_KEY || key == B_F3_KEY || key == B_F5_KEY || key == B_F6_KEY)
    ShowLevel(key == B_F5_KEY || key == B_F6_KEY, result != 0);
  }
 }
};
class FnFilter : public BInputServerFilter {
 Actions* fActions; bool fReady; bool fHeld[128]; bool fPass[128]; uint32 fAltMask;
public:
 FnFilter() : fActions(new Actions) {
  memset(fHeld, 0, sizeof(fHeld)); memset(fPass, 0, sizeof(fPass));
  // Physical left/right Alt positions, using this machine's actual modifier map.
  fAltMask = 0;
  key_map* map = NULL; char* chars = NULL;
  get_key_map(&map, &chars);
  if (map) {
   const uint32 keys[] = {map->left_shift_key, map->right_shift_key,
    map->left_command_key, map->right_command_key, map->left_control_key,
    map->right_control_key, map->left_option_key, map->right_option_key};
   const uint32 masks[] = {B_LEFT_SHIFT_KEY, B_RIGHT_SHIFT_KEY,
    B_LEFT_COMMAND_KEY, B_RIGHT_COMMAND_KEY, B_LEFT_CONTROL_KEY,
    B_RIGHT_CONTROL_KEY, B_LEFT_OPTION_KEY, B_RIGHT_OPTION_KEY};
   for (int i = 0; i < 8; ++i)
    if (keys[i] == 0x5d || keys[i] == 0x5f) fAltMask |= masks[i];
  } else fAltMask = B_LEFT_COMMAND_KEY | B_RIGHT_COMMAND_KEY;
  free(map); free(chars);
  fReady = fActions->Run() >= B_OK;
  Log("filter loaded", fReady ? B_OK : B_ERROR);
 }
 ~FnFilter() { if (fActions->Lock()) fActions->Quit(); }
 status_t InitCheck() { return fReady ? B_OK : B_ERROR; }
 filter_result Filter(BMessage* message, BList*) {
  bool down = message->what == B_KEY_DOWN || message->what == B_UNMAPPED_KEY_DOWN;
  bool up = message->what == B_KEY_UP || message->what == B_UNMAPPED_KEY_UP;
  int32 key;
  if ((!down && !up) || message->FindInt32("key", &key) != B_OK) return B_DISPATCH_MESSAGE;
  bool functionKey = key >= B_F1_KEY && key <= B_F12_KEY;
  if (!functionKey && key != kComma && key != kPeriod) return B_DISPATCH_MESSAGE;
  int index = key;
  int32 modifiers = 0;
  message->FindInt32("modifiers", &modifiers);
  // Keep a whole press/repeat/release sequence on the same route even if Alt
  // changes while the function key is held, avoiding orphaned application events.
  if (!fHeld[index]) {
   bool alt = (modifiers & fAltMask) != 0;
   fPass[index] = functionKey ? alt : !alt;
  }
  bool pass = fPass[index];
  bool repeatable = key == B_F2_KEY || key == B_F3_KEY || key == B_F5_KEY || key == B_F6_KEY;
  if (!pass && down && (!fHeld[index] || repeatable)) { BMessage action(kAction); action.AddInt32("key", key); fActions->PostMessage(&action); }
  fHeld[index] = down;
  return pass ? B_DISPATCH_MESSAGE : B_SKIP_MESSAGE;
 }
};
extern "C" BInputServerFilter* instantiate_input_filter() { return new FnFilter; }
