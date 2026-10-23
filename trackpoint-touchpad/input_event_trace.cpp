// Temporary pointer-only input filter. It observes events without changing them.
#include <InputServerFilter.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <OS.h>
#include <cstdio>

class PointerTrace : public BInputServerFilter {
public:
    PointerTrace()
        : fFile(fopen("/boot/home/x270-camera-mic/ps2-events.log", "a")),
          fLastButtons(0), fCount(0)
    {
        if (fFile) { fprintf(fFile, "TRACE START %lld\n", system_time()); fflush(fFile); }
    }
    ~PointerTrace() { if (fFile) fclose(fFile); }
    filter_result Filter(BMessage* message, BList*) override
    {
        if (!fFile || fCount >= 10000) return B_DISPATCH_MESSAGE;
        const char* type = message->what == B_MOUSE_DOWN ? "DOWN"
            : message->what == B_MOUSE_UP ? "UP"
            : message->what == B_MOUSE_MOVED ? "MOVE" : nullptr;
        if (!type) return B_DISPATCH_MESSAGE;
        int32 buttons = -1, clicks = -1, subtype = -1;
        int64 when = -1;
        BPoint where(-1, -1);
        message->FindInt32("buttons", &buttons);
        message->FindInt32("clicks", &clicks);
        message->FindInt32("be:device_subtype", &subtype);
        message->FindInt64("when", &when);
        message->FindPoint("where", &where);
        // Include unheld moves too: they reveal a release implied by movement.
        fprintf(fFile, "%s when=%lld buttons=%lx previous=%lx clicks=%ld subtype=%ld where=%.0f,%.0f\n",
            type, when, (unsigned long)buttons, (unsigned long)fLastButtons,
            (long)clicks, (long)subtype, where.x, where.y);
        fLastButtons = buttons;
        ++fCount;
        fflush(fFile);
        return B_DISPATCH_MESSAGE;
    }
private:
    FILE* fFile;
    uint32 fLastButtons;
    unsigned fCount;
};

extern "C" BInputServerFilter* instantiate_input_filter()
{
    return new PointerTrace;
}
