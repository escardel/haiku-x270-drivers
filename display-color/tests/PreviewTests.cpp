#define DISPLAYCOLOR_TESTING
#define main DisplayColorAppMain
#include "../DisplayColor.cpp"
#undef main

#include <cstdlib>

int PreviewSettingsSelfTest()
{
	char directory[] = "/tmp/displaycolor-preview-settings-XXXXXX";
	if (!mkdtemp(directory)) return 1;
	BString path(directory);
	path << "/settings";
	gTestSettingsPath = path.String();
	int result = 0;
	{
		ColorApp app;
		if (app.InitCheck() != B_OK) return 2;
		app.fContrast = 104;
		app.fGamma = 80;
		app.fRedSaturation = 120;
		app.fFilter.enabled = true;
		if (app.Save() != B_OK) result = 3;
		char name[] = "DisplayColor", preview[] = "--preview";
		char* arguments[] = {name, preview};
		app.ArgvReceived(2, arguments);
		if (!app.fPreview) result = 4;
		app.fContrast = 150;
		app.fGamma = 200;
		app.fRedSaturation = 0;
		app.fFilter.enabled = false;
		if (app.Save() != B_OK) result = 5;
		BMessage startup(kStartup);
		app.MessageReceived(&startup);
		BFile file(path.String(), B_READ_ONLY);
		BMessage saved;
		int32 contrast = 0, gamma = 0, red = 0;
		bool enabled = false;
		if (saved.Unflatten(&file) != B_OK
			|| saved.FindInt32("contrast", &contrast) != B_OK || contrast != 104
			|| saved.FindInt32("gamma", &gamma) != B_OK || gamma != 80
			|| saved.FindInt32("saturation_red", &red) != B_OK || red != 120
			|| saved.FindBool("filter_enabled", &enabled) != B_OK || !enabled)
			result = 6;
	}
	unlink(path.String());
	rmdir(directory);
	gTestSettingsPath = nullptr;
	if (!result)
		puts("PASS: normal settings persistence, temporary app preview does not save, and startup actions are ignored (isolated settings; no hardware access).");
	return result;
}

int main()
{
	return PreviewSettingsSelfTest();
}
