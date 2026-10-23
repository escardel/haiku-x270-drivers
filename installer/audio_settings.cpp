// SPDX-License-Identifier: MIT
// Run with media_addon_server stopped. Preserve unrelated mixer settings.
#include <File.h>
#include <Message.h>
#include <TypeConstants.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

static status_t Set(BMessage& audio, int32 id, const void* data, size_t size)
{
	for (int32 i = 0, found; audio.FindInt32("parameterID", i, &found) == B_OK; i++) {
		if (found == id)
			return audio.ReplaceData("parameterData", B_RAW_TYPE, i, data, size);
	}
	status_t status = audio.AddInt32("parameterID", id);
	return status == B_OK ? audio.AddData("parameterData", B_RAW_TYPE, data, size) : status;
}

int main(int argc, char** argv)
{
	if (argc != 2) return 2;
	BMessage root, audio;
	BFile input(argv[1], B_READ_ONLY);
	if (input.InitCheck() != B_OK && input.InitCheck() != B_ENTRY_NOT_FOUND) return 3;
	if (input.InitCheck() == B_OK && root.Unflatten(&input) != B_OK) return 3;
	bool existing = root.FindMessage("HD Audio", &audio) == B_OK;
	int32 ids = 0, values = 0;
	type_code type;
	audio.GetInfo("parameterID", &type, &ids);
	audio.GetInfo("parameterData", &type, &values);
	if (ids != values) { fprintf(stderr, "Unbalanced mixer parameter arrays\n"); return 4; }
	float gain[2] = {12, 12};
	int32 selector = 5;
	if (Set(audio, 140, gain, sizeof(gain)) != B_OK
		|| Set(audio, 142, &selector, sizeof(selector)) != B_OK) return 5;
	status_t status = existing ? root.ReplaceMessage("HD Audio", &audio)
		: root.AddMessage("HD Audio", &audio);
	if (status != B_OK) return 6;
	char temp[1024];
	if (snprintf(temp, sizeof(temp), "%s.x270-new", argv[1]) >= int(sizeof(temp))) return 7;
	BFile output(temp, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (output.InitCheck() != B_OK || root.Flatten(&output) != B_OK || output.Sync() != B_OK)
		return 8;
	output.Unset();
	if (rename(temp, argv[1]) != 0) return 9;
	puts("Saved X270 internal microphone: ADC9 selector 5, stereo gain 12 dB.");
	return 0;
}
