#define main DisplayColorAppMain
#include "DisplayColor.cpp"
#undef main

#include <Alert.h>
#include <Invoker.h>
#include <cstdlib>
#include <image.h>

extern char** environ;

enum {
	kSafeStart = 'tstr', kSafeKeep = 'keep', kSafeRevert = 'rvrt', kSafeStop = 'stop',
	kSafeReady = 'rdy!', kSafeApplied = 'apld', kSafeKept = 'kept',
	kSafeRestored = 'rstd', kSafeStopped = 'done', kSafeError = 'err!',
	kSafePoll = 'poll', kSafeDecision = 'dcsn', kSafeCountdown = 'cntd',
	kSafeLaunchTest = 'lnch'
};

enum { kRehearsal, kIdentity, kRed, kGreen, kBlue, kFilter, kStartupPreview,
	kGrayscale, kTestCount };

struct PreviewRequest {
	int32 token = 0, test = kRehearsal;
};

struct PreviewResponse {
	port_id commands = -1;
	int32 token = 0, test = kRehearsal;
	team_id child = -1;
	status_t result = B_OK;
	bigtime_t deadline = 0;
	bool dirty = false, expired = false;
};

static const char* TestName(int test)
{
	switch (test) {
		case kIdentity: return "RGB identity + plane-gamma bypass";
		case kRed: return "Red saturation 50%";
		case kGreen: return "Green saturation 50%";
		case kBlue: return "Blue saturation 50%";
		case kFilter: return "Blue light filter 15%";
		case kStartupPreview: return "App sliders (guarded 12-second preview)";
		case kGrayscale: return "All RGB 0% — whole display grayscale";
		default: return "Timeout rehearsal (no display change)";
	}
}

static void RecordPreview(const char* logPath, int32 state, const PreviewResponse& response)
{
	FILE* log = fopen(logPath, "a");
	if (!log) return;
	fprintf(log, "%lld state=%c%c%c%c test=%d token=%d result=%d dirty=%d expired=%d variant=csc-frame-latch-proxy\n",
		(long long)system_time(), (state >> 24) & 255, (state >> 16) & 255,
		(state >> 8) & 255, state & 255, response.test, response.token,
		response.result, response.dirty, response.expired);
	fclose(log);
}

static int PreviewWatchdog(team_id parent, port_id replies, bigtime_t duration,
	bool simulate, const char* logPath, const char* appPath)
{
	Hardware hardware;
	HardwareSnapshot original;
	BRoster roster;
	PreviewResponse response;
	bool recoveryPending = false;
	if (!simulate) {
		response.result = hardware.Init();
		if (response.result == B_OK) response.result = hardware.Capture(original);
		if (response.result == B_OK && hardware.PanelSelfRefreshEnabled())
			response.result = B_NOT_SUPPORTED;
	}
	response.commands = create_port(16, "Display Color preview guard");
	auto report = [&](int32 state) {
		write_port_etc(replies, state, &response, sizeof(response), B_RELATIVE_TIMEOUT, 0);
		RecordPreview(logPath, state, response);
	};
	if (response.result != B_OK || response.commands < 0) {
		if (response.result == B_OK) response.result = response.commands;
		report(kSafeError);
		if (response.commands >= 0) delete_port(response.commands);
		return 1;
	}
	auto restore = [&]() {
		if (response.child >= 0) {
			team_info childInfo;
			if (get_team_info(response.child, &childInfo) == B_OK) {
				kill_team(response.child);
				for (int attempt = 0; attempt < 100; attempt++) {
					if (get_team_info(response.child, &childInfo) != B_OK) break;
					snooze(10000);
				}
				if (get_team_info(response.child, &childInfo) == B_OK) {
					response.result = B_BUSY;
					recoveryPending = true;
					return;
				}
			}
			response.child = -1;
		}
		response.result = !response.dirty || simulate ? B_OK : hardware.Restore(original);
		if (response.result == B_OK) response.dirty = false;
		recoveryPending = response.result != B_OK;
		response.deadline = 0;
	};
	report(kSafeReady);
	bool pending = false;
	for (;;) {
		team_info parentInfo;
		if (get_team_info(parent, &parentInfo) != B_OK) {
			restore();
			if (response.result == B_OK) {
				report(kSafeStopped);
				break;
			}
			report(kSafeError);
			snooze(100000);
			continue;
		}
		if (recoveryPending) {
			snooze(100000);
			restore();
			if (!recoveryPending) {
				pending = false;
				report(kSafeRestored);
			}
			continue;
		}
		if (pending && response.child >= 0) {
			team_info childInfo;
			if (get_team_info(response.child, &childInfo) != B_OK) {
				restore();
				pending = false;
				report(kSafeRestored);
			}
		}
		if (pending && system_time() >= response.deadline) {
			restore();
			pending = false;
			response.expired = true;
			report(kSafeRestored);
			response.expired = false;
		}
		unsigned char message[sizeof(PreviewAdjustment)];
		int32 command;
		ssize_t size = read_port_etc(response.commands, &command, message,
			sizeof(message), B_RELATIVE_TIMEOUT, 20000);
		if (size < 0) {
			if (size == B_TIMED_OUT || size == B_INTERRUPTED || size == B_WOULD_BLOCK)
				continue;
			restore();
			report(kSafeStopped);
			break;
		}
		if (command == kPreviewAdjust && size == sizeof(PreviewAdjustment)) {
			PreviewAdjustment adjustment;
			memcpy(&adjustment, message, sizeof(adjustment));
			PreviewAdjustmentResult result;
			if (pending && response.test == kStartupPreview && response.child >= 0
				&& adjustment.token == response.token && system_time() < response.deadline) {
				result.status = simulate ? B_OK : hardware.Apply(
					std::clamp((int)adjustment.contrast, 50, 200),
					std::clamp((int)adjustment.gamma, 50, 300),
					std::clamp((int)adjustment.red, 0, 200),
					std::clamp((int)adjustment.green, 0, 200),
					std::clamp((int)adjustment.blue, 0, 200),
					std::clamp((int)adjustment.filter, 0, 100), &result.changed);
			}
			write_port_etc(adjustment.replies, kPreviewAdjusted, &result, sizeof(result),
				B_RELATIVE_TIMEOUT, 0);
			PreviewResponse event = response;
			event.result = result.status;
			RecordPreview(logPath, kPreviewAdjust, event);
			continue;
		}
		if (size != sizeof(PreviewRequest)) continue;
		PreviewRequest request;
		memcpy(&request, message, sizeof(request));
		if (command == kSafeStop) {
			restore();
			if (response.result == B_OK) {
				report(kSafeStopped);
				break;
			}
			report(kSafeError);
			continue;
		}
		if (command == kSafeRevert) {
			restore();
			pending = false;
			report(kSafeRestored);
			continue;
		}
		if (command == kSafeKeep) {
			if (!pending || request.token != response.token
				|| system_time() >= response.deadline) {
				if (pending) {
					restore();
					pending = false;
				}
				report(kSafeRestored);
				continue;
			}
			pending = false;
			response.deadline = 0;
			if (response.test == kStartupPreview) restore();
			report(kSafeKept);
			continue;
		}
		if (command != kSafeStart || request.test < kRehearsal || request.test >= kTestCount)
			continue;
		restore();
		pending = false;
		if (response.result != B_OK) { report(kSafeError); continue; }
		response.token = request.token;
		response.test = request.test;
		if (!simulate) {
			struct stat startup;
			if (lstat(StartupPath().Path(), &startup) == 0
				|| roster.IsRunning(kSignature)) {
				response.result = B_BUSY;
				report(kSafeError);
				continue;
			}
		}
		response.deadline = system_time() + duration;
		response.dirty = request.test != kRehearsal;
		if (request.test == kStartupPreview) {
			const char* executable = simulate ? "/bin/sleep" : appPath;
			BString commandsText, tokenText;
			commandsText << response.commands; tokenText << response.token;
			const char* arguments[] = {executable, simulate ? "60" : "--preview",
				"--preview-port", commandsText.String(), "--preview-token", tokenText.String()};
			if (!simulate && access(executable, X_OK) != 0)
				response.result = B_ENTRY_NOT_FOUND;
			else {
				thread_id child = load_image(simulate ? 2 : 6, arguments, (const char**)environ);
				thread_info childInfo;
				if (child < 0) response.result = child;
				else if (get_thread_info(child, &childInfo) != B_OK) {
					kill_thread(child);
					response.result = B_ERROR;
				} else {
					response.child = childInfo.team;
					response.result = resume_thread(child);
				}
			}
		} else if (!simulate && response.dirty) {
			BMessage settings;
			BFile file(SettingsPath().Path(), B_READ_ONLY);
			int32 contrast = 100, gamma = 100;
			if (settings.Unflatten(&file) == B_OK) {
				int32 value;
				if (settings.FindInt32("contrast", &value) == B_OK) contrast = value;
				if (settings.FindInt32("gamma", &value) == B_OK) gamma = value;
			}
			bool grayscale = request.test == kGrayscale;
			response.result = hardware.Apply(std::clamp((int)contrast, 50, 200),
				std::clamp((int)gamma, 50, 300), grayscale ? 0 : request.test == kRed ? 50 : 100,
				grayscale ? 0 : request.test == kGreen ? 50 : 100,
				grayscale ? 0 : request.test == kBlue ? 50 : 100,
				request.test == kFilter ? 15 : 0, nullptr, request.test == kIdentity, true);
		}
		if (response.result != B_OK || system_time() >= response.deadline) {
			status_t applied = response.result;
			restore();
			if (applied != B_OK) response.result = applied;
			report(kSafeError);
			continue;
		}
		pending = true;
		report(kSafeApplied);
	}
	delete_port(response.commands);
	return response.result == B_OK ? 0 : 1;
}

static thread_id StartPreviewGuard(const char* executable, team_id parent,
	port_id replies, int durationMs, bool simulate, const char* logPath,
	const char* appPath = "")
{
	BString parentText, repliesText, durationText;
	parentText << parent; repliesText << replies; durationText << durationMs;
	const char* arguments[] = {executable, "--watchdog", parentText.String(),
		repliesText.String(), durationText.String(), simulate ? "simulate" : "hardware",
		logPath, appPath};
	thread_id child = load_image(8, arguments, (const char**)environ);
	if (child >= 0) {
		status_t result = resume_thread(child);
		if (result != B_OK) { kill_thread(child); return result; }
	}
	return child;
}

class PreviewAlert : public BAlert {
	BString fName;
	bool fRehearsal;
	bool fStartupPreview;
public:
	PreviewAlert(const char* name, bool rehearsal, bool startupPreview)
		: BAlert("Display Color: confirm this test",
			rehearsal ? "Timeout rehearsal only — no display change.\n"
				"Please wait for this dialog to close automatically after 12 seconds."
				: "Preparing a protected display test. If the screen goes black, wait.\n"
				"The independent guard will restore the original display in 12 seconds.\n\n"
				"Click OK only if the display looks correct.",
			"Revert now", rehearsal ? nullptr : "OK, keep this test", nullptr,
			B_WIDTH_AS_USUAL, B_WARNING_ALERT),
		fName(name), fRehearsal(rehearsal), fStartupPreview(startupPreview)
	{
		SetDefaultButton(ButtonAt(0));
		SetShortcut(0, B_ESCAPE);
		if (ButtonAt(1)) ButtonAt(1)->SetEnabled(false);
	}
	void MessageReceived(BMessage* message) override
	{
		if (message->what == kSafeCountdown) {
			int32 seconds = 0;
			message->FindInt32("seconds", &seconds);
			if (ButtonAt(1))
				ButtonAt(1)->SetEnabled(seconds > 0 && seconds <= (fStartupPreview ? 4 : 10));
			BString text;
			if (fRehearsal)
				text.SetToFormat("Timeout rehearsal — no display change.\n\n"
					"Please wait %d seconds for this dialog to close automatically.\n"
					"This verifies the independent rollback deadline before hardware tests.\n\n"
					"Canceling early leaves hardware tests locked.", seconds);
			else if (fStartupPreview)
				text.SetToFormat("Guarded app controls preview\n\n"
					"Try an RGB slider and watch the colored reference patches.\n"
					"Changes are temporary and login startup is locked off.\n"
					"If it goes black at any point, wait for the automatic rollback.\n"
					"The app closes and the display is restored in %d seconds.\n\n"
					"Click OK only if the screen stayed normal throughout.",
					seconds);
			else
				text.SetToFormat("%s\n\nDoes the display look correct?\n"
					"Original display returns automatically in %d seconds unless you click OK.\n"
					"If the screen is black, do nothing and wait.\n\n"
					"Enter or Escape reverts. Testing never saves settings or enables startup.",
					fName.String(), seconds);
			SetText(text.String());
		} else BAlert::MessageReceived(message);
	}
};

class PreviewWindow : public BWindow {
public:
	PreviewWindow() : BWindow(BRect(100, 100, 700, 550), "Display Color — protected tests",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE) {}
	bool QuitRequested() override
	{
		be_app->PostMessage(kSafeStop);
		return false;
	}
};

class SafePreviewApp : public BApplication {
	port_id fReplies = -1, fCommands = -1;
	thread_id fGuard = -1;
	BMessageRunner* fPoll = nullptr;
	PreviewWindow* fWindow = nullptr;
	BStringView* fStatus = nullptr;
	BButton* fTests[kTestCount] = {};
	BMessenger fAlert;
	int32 fToken = 0, fTest = kRehearsal;
	bigtime_t fDeadline = 0;
	bool fRehearsed = false, fIdentityConfirmed = false, fStopping = false;
	bool fSimulate;
	void EnableTests(bool enabled)
	{
		for (int test = 0; test < kTestCount; test++)
			fTests[test]->SetEnabled(enabled && (test == kRehearsal
				|| (fRehearsed && (test == kIdentity || test == kFilter
					|| test == kStartupPreview || fIdentityConfirmed))));
	}
	status_t Send(int32 command)
	{
		PreviewRequest request;
		request.token = fToken; request.test = fTest;
		return write_port_etc(fCommands, command, &request, sizeof(request),
			B_RELATIVE_TIMEOUT, 100000);
	}
	void CloseAlert()
	{
		if (fAlert.IsValid()) fAlert.SendMessage(B_QUIT_REQUESTED);
		fAlert = BMessenger();
	}
	void Poll()
	{
		for (;;) {
			PreviewResponse response;
			int32 state;
			if (read_port_etc(fReplies, &state, &response, sizeof(response),
				B_RELATIVE_TIMEOUT, 0) != sizeof(response)) break;
			if (state == kSafeReady) {
				fCommands = response.commands;
				if (fWindow->Lock()) {
					fStatus->SetText("Guard ready. First run the timeout rehearsal and let it expire.");
					EnableTests(true);
					fWindow->Unlock();
				}
			} else if (state == kSafeApplied) {
				fDeadline = response.deadline;
				if (fWindow->Lock()) {
					fStatus->SetText(response.test == kRehearsal
						? "Rehearsal: wait for the countdown to finish. No display changes."
						: "Test active. Click OK only if visible and correct; otherwise wait.");
					fWindow->Unlock();
				}
			} else {
				fDeadline = 0;
				CloseAlert();
				if (state == kSafeRestored && response.test == kRehearsal && response.expired)
					fRehearsed = true;
				if (state == kSafeKept && response.test == kIdentity)
					fIdentityConfirmed = true;
				if (fWindow->Lock()) {
					BString text;
					if (response.result != B_OK)
						text.SetToFormat("Test stopped (status %d). Do not continue if display is incorrect.", response.result);
					else if (state == kSafeKept && response.test == kStartupPreview)
						text = "App preview confirmed; app closed and original display restored.";
					else if (state == kSafeKept)
						text = "Confirmed for this session only. Restore or exit returns the original display.";
					else text = "Original display restored. Nothing was saved; startup remains disabled.";
					fStatus->SetText(text.String());
					EnableTests(response.result == B_OK && state != kSafeError
						&& state != kSafeStopped && !fStopping);
					fWindow->Unlock();
				}
				if (state == kSafeStopped) {
					if (fWindow->Lock()) { fWindow->Quit(); fWindow = nullptr; }
					PostMessage(B_QUIT_REQUESTED);
				}
			}
		}
		if (fDeadline > 0 && fAlert.IsValid()) {
			BMessage countdown(kSafeCountdown);
			countdown.AddInt32("seconds", (int32)std::max((bigtime_t)0,
				(fDeadline - system_time() + 999999) / 1000000));
			fAlert.SendMessage(&countdown);
		}
	}
public:
	explicit SafePreviewApp(bool simulate)
		: BApplication("application/x-vnd.DisplayColor-safe-test"), fSimulate(simulate) {}
	~SafePreviewApp()
	{
		delete fPoll;
		if (fReplies >= 0) delete_port(fReplies);
	}
	void ReadyToRun() override
	{
		fWindow = new PreviewWindow;
		fStatus = new BStringView("guard_status", "Starting independent rollback guard…");
		BLayoutBuilder::Group<> buttons(fWindow, B_VERTICAL, 12);
		buttons.SetInsets(16)
			.Add(new BStringView("notice", fSimulate ? "SIMULATION — no display hardware is changed."
				: "Attended tests only. Startup is disabled; saved settings are not modified."))
			.Add(new BStringView("instructions", "Each test expires after 12 seconds unless you click OK."))
			.Add(new BStringView("recovery", "Black screen? Wait. Closing this tool restores the original display."))
			.Add(new ColorReference())
			.Add(new BStringView("rgb_hint", "Grayscale test: these patches and the whole desktop should lose their color."));
		for (int test = 0; test < kTestCount; test++) {
			BMessage* start = new BMessage(kSafeLaunchTest);
			start->AddInt32("test", test);
			fTests[test] = new BButton(TestName(test), TestName(test), start);
			fTests[test]->SetTarget(this);
			buttons.Add(fTests[test]);
		}
		BButton* restore = new BButton("Restore original display", new BMessage(kSafeRevert));
		BButton* stop = new BButton("Restore and end testing", new BMessage(kSafeStop));
		restore->SetTarget(this); stop->SetTarget(this);
		buttons.Add(fStatus).Add(restore).Add(stop);
		EnableTests(false);
		fWindow->CenterOnScreen();
		fWindow->Show();
		fWindow->Activate(true);
		fReplies = create_port(32, "Display Color preview replies");
		app_info info;
		BPath executable;
		BEntry entry;
		if (fReplies < 0 || GetAppInfo(&info) != B_OK
			|| entry.SetTo(&info.ref) != B_OK || entry.GetPath(&executable) != B_OK) {
			fStatus->SetText("Could not start guard. No display changes were made.");
			return;
		}
		BPath log(executable);
		log.GetParent(&log); log.Append("safe-test-events.log");
		BPath app(executable);
		app.GetParent(&app); app.Append("Display Color");
		if (access(app.Path(), X_OK) != 0) {
			app.GetParent(&app); app.Append("DisplayColor");
		}
		fGuard = StartPreviewGuard(executable.Path(), getpid(), fReplies,
			12000, fSimulate, log.Path(), app.Path());
		BMessage poll(kSafePoll);
		fPoll = new BMessageRunner(BMessenger(this), &poll, 100000);
	}
	bool QuitRequested() override
	{
		if (fCommands >= 0 && !fStopping) { fStopping = true; Send(kSafeStop); return false; }
		return fWindow == nullptr || fGuard < 0;
	}
	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kSafePoll: Poll(); break;
			case kSafeLaunchTest: {
				if (fCommands < 0 || fDeadline != 0 || fStopping) break;
				message->FindInt32("test", &fTest);
				if (fTest < 0 || fTest >= kTestCount || !fTests[fTest]->IsEnabled()) break;
				fToken++;
				if (fWindow->Lock()) { EnableTests(false); fWindow->Unlock(); }
				PreviewAlert* alert = new PreviewAlert(TestName(fTest),
					fTest == kRehearsal, fTest == kStartupPreview);
				fAlert = BMessenger(alert);
				BMessage* decision = new BMessage(kSafeDecision);
				decision->AddInt32("token", fToken);
				alert->Go(new BInvoker(decision, BMessenger(this)));
				if (Send(kSafeStart) != B_OK) CloseAlert();
				break;
			}
			case kSafeDecision: {
				int32 which = 0, token = 0;
				if (message->FindInt32("token", &token) != B_OK || token != fToken)
					break;
				message->FindInt32("which", &which);
				fAlert = BMessenger();
				Send(which == 1 && fDeadline > 0 ? kSafeKeep : kSafeRevert);
				break;
			}
			case kSafeRevert: Send(kSafeRevert); break;
			case kSafeStop:
				fStopping = true;
				CloseAlert();
				if (fCommands >= 0) Send(kSafeStop);
				else { fWindow->Lock(); fWindow->Quit(); fWindow = nullptr; PostMessage(B_QUIT_REQUESTED); }
				break;
			default: BApplication::MessageReceived(message);
		}
	}
};

static int PreviewSelfTest(const char* executable)
{
	uint32 originalPlane = 0xc4000000;
	uint32 candidatePlane = ColorPlaneControl(originalPlane, true, true);
	if (candidatePlane != 0xc4802000
		|| ColorPlaneControl(originalPlane, true, false) != 0xc4800000
		|| ColorPlaneControl(originalPlane, false, true) != originalPlane
		|| ((candidatePlane & ~0x40802000u) | (originalPlane & 0x40802000u)) != originalPlane)
		return 15;
	const char* log = "/tmp/displaycolor-guard-selftest.log";
	port_id replies = create_port(32, "Display Color guard self-test");
	if (replies < 0) return 1;
	auto receive = [&](int32 expected, PreviewResponse& response) {
		int32 state = 0;
		return read_port_etc(replies, &state, &response, sizeof(response),
			B_RELATIVE_TIMEOUT, 5000000) == sizeof(response) && state == expected;
	};
	thread_id child = StartPreviewGuard(executable, getpid(), replies, 350, true, log);
	PreviewResponse response;
	if (child < 0 || !receive(kSafeReady, response)) return 2;
	port_id commands = response.commands;
	PreviewRequest request;
	request.token = 1; request.test = kIdentity;
	write_port(commands, kSafeStart, &request, sizeof(request));
	if (!receive(kSafeApplied, response) || !response.dirty) return 3;
	if (!receive(kSafeRestored, response) || response.dirty || !response.expired) return 4;
	write_port(commands, kSafeKeep, &request, sizeof(request));
	if (!receive(kSafeRestored, response) || response.dirty) return 5;
	request.token++;
	write_port(commands, kSafeStart, &request, sizeof(request));
	if (!receive(kSafeApplied, response)) return 6;
	PreviewRequest stale = request; stale.token--;
	write_port(commands, kSafeKeep, &stale, sizeof(stale));
	if (!receive(kSafeRestored, response) || response.dirty) return 7;
	request.token++;
	write_port(commands, kSafeStart, &request, sizeof(request));
	if (!receive(kSafeApplied, response)) return 8;
	write_port(commands, kSafeKeep, &request, sizeof(request));
	if (!receive(kSafeKept, response) || !response.dirty) return 9;
	write_port(commands, kSafeStop, &request, sizeof(request));
	if (!receive(kSafeStopped, response) || response.dirty) return 10;
	status_t exitStatus;
	wait_for_thread(child, &exitStatus);
	child = StartPreviewGuard(executable, getpid(), replies, 350, true, log);
	if (child < 0 || !receive(kSafeReady, response)) return 16;
	commands = response.commands;
	request.token++; request.test = kStartupPreview;
	write_port(commands, kSafeStart, &request, sizeof(request));
	if (!receive(kSafeApplied, response) || response.child < 0) return 17;
	PreviewAdjustment adjustment;
	adjustment.token = response.token;
	adjustment.red = 50;
	int adjusted = -1;
	if (ApplyThroughGuard(commands, adjustment, adjusted) != B_OK || adjusted != 0)
		return 23;
	adjustment.token++;
	if (ApplyThroughGuard(commands, adjustment, adjusted) != B_NOT_ALLOWED)
		return 24;
	team_id previewTeam = response.child;
	if (!receive(kSafeRestored, response) || response.dirty || !response.expired
		|| response.child >= 0) return 18;
	team_info previewInfo;
	if (get_team_info(previewTeam, &previewInfo) == B_OK) return 19;
	adjustment.token = response.token;
	if (ApplyThroughGuard(commands, adjustment, adjusted) != B_NOT_ALLOWED)
		return 25;
	request.token++;
	write_port(commands, kSafeStart, &request, sizeof(request));
	if (!receive(kSafeApplied, response) || response.child < 0) return 20;
	previewTeam = response.child;
	write_port(commands, kSafeKeep, &request, sizeof(request));
	if (!receive(kSafeKept, response) || response.dirty || response.child >= 0
		|| get_team_info(previewTeam, &previewInfo) == B_OK) return 21;
	write_port(commands, kSafeStop, &request, sizeof(request));
	if (!receive(kSafeStopped, response)) return 22;
	wait_for_thread(child, &exitStatus);
	const char* sleeperArguments[] = {"/bin/sleep", "60"};
	thread_id sleeper = load_image(2, sleeperArguments, (const char**)environ);
	if (sleeper < 0 || resume_thread(sleeper) != B_OK) return 11;
	child = StartPreviewGuard(executable, sleeper, replies, 12000, true, log);
	if (child < 0 || !receive(kSafeReady, response)) { kill_team(sleeper); return 12; }
	commands = response.commands;
	request.test = kStartupPreview;
	write_port(commands, kSafeStart, &request, sizeof(request));
	if (!receive(kSafeApplied, response) || response.child < 0) { kill_team(sleeper); return 13; }
	previewTeam = response.child;
	kill_team(sleeper);
	if (!receive(kSafeStopped, response) || response.dirty || response.child >= 0
		|| get_team_info(previewTeam, &previewInfo) == B_OK) return 14;
	wait_for_thread(child, &exitStatus);
	delete_port(replies);
	puts("PASS: rollback, deadlines, stale-confirmation rejection, guard-owned adjustments, expired/stale adjustment rejection, child cleanup and parent-crash rollback (simulated; no hardware writes).");
	return 0;
}

int main(int argc, char** argv)
{
	if (argc == 2 && !strcmp(argv[1], "--launch-simulation")) {
		entry_ref executable;
		if (get_ref_for_path(argv[0], &executable) != B_OK) return 1;
		const char* arguments[] = {"--simulate"};
		status_t result = be_roster->Launch(&executable, 1, arguments);
		return result == B_OK ? 0 : 1;
	}
	if (argc == 8 && !strcmp(argv[1], "--watchdog")) {
		int durationMs = atoi(argv[4]);
		if (durationMs < 250 || durationMs > 15000) return 1;
		return PreviewWatchdog(atoi(argv[2]), atoi(argv[3]), durationMs * 1000LL,
			!strcmp(argv[5], "simulate"), argv[6], argv[7]);
	}
	if (argc == 2 && !strcmp(argv[1], "--self-test")) return PreviewSelfTest(argv[0]);
	SafePreviewApp application(argc == 2 && !strcmp(argv[1], "--simulate"));
	if (application.InitCheck() != B_OK) return 1;
	application.Run();
	return 0;
}
