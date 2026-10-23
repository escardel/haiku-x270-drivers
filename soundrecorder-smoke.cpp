#include <Application.h>
#include <OS.h>

#include "RecorderWindow.h"

#include <cstdio>

static int32
record_once(void* data)
{
    RecorderWindow* window = static_cast<RecorderWindow*>(data);
    snooze(1000000);
    printf("SoundRecorder smoke: record\n");
    window->PostMessage(RecorderWindow::RECORD);
    snooze(5000000);
    printf("SoundRecorder smoke: stop\n");
    window->PostMessage(RecorderWindow::STOP);
    snooze(30000000);
    be_app->PostMessage(B_QUIT_REQUESTED);
    return 0;
}

int
main()
{
    BApplication app("application/x-vnd.Haiku-SoundRecorder-X270-Smoke");
    RecorderWindow* window = new RecorderWindow();
    status_t status = window->InitCheck();
    printf("SoundRecorder smoke: init=%ld\n", status);
    if (status != B_OK)
        return 1;
    thread_id thread = spawn_thread(record_once, "record_once",
        B_NORMAL_PRIORITY, window);
    if (thread < B_OK)
        return 2;
    resume_thread(thread);
    app.Run();
    return 0;
}
