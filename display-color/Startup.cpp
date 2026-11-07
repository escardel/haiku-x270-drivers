#include <Application.h>
#include <Entry.h>
#include <Path.h>
#include <Roster.h>
int main()
{
	BApplication app("application/x-vnd.DisplayColor-startup");
	const char* args[] = {"--background"};
	app_info info;
	if (app.GetAppInfo(&info) != B_OK) return 1;
	BEntry helper(&info.ref, true);
	BPath application;
	if (helper.GetPath(&application) != B_OK
		|| application.GetParent(&application) != B_OK
		|| application.Append("Display Color") != B_OK)
		return 1;
	entry_ref ref;
	if (get_ref_for_path(application.Path(), &ref) != B_OK) {
		if (application.GetParent(&application) != B_OK
			|| application.Append("DisplayColor") != B_OK
			|| get_ref_for_path(application.Path(), &ref) != B_OK)
			return 1;
	}
	status_t status = be_roster->Launch(&ref, 1, args);
	return status == B_OK || status == B_ALREADY_RUNNING ? 0 : 1;
}
