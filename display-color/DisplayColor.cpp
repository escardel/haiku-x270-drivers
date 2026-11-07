#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <Slider.h>
#include <String.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <Window.h>
#include <intel_extreme.h>
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static const char* kSignature = "application/x-vnd.DisplayColor";
enum { kChange = 'chng', kReset = 'rset', kStartup = 'strt',
	kHide = 'hide', kTick = 'tick', kExit = 'exit' };

enum { kPreviewAdjust = 'padj', kPreviewAdjusted = 'pres' };
struct PreviewAdjustment {
	port_id replies = -1;
	int32 token = 0;
	int32 contrast = 100, gamma = 100, red = 100, green = 100, blue = 100, filter = 0;
};
struct PreviewAdjustmentResult {
	status_t status = B_NOT_ALLOWED;
	int32 changed = 0;
};

static status_t ApplyThroughGuard(port_id commands, PreviewAdjustment& request, int& changed)
{
	if (commands < 0) return B_NOT_SUPPORTED;
	request.replies = create_port(1, "Display Color temporary adjustment reply");
	if (request.replies < 0) return request.replies;
	status_t status = write_port_etc(commands, kPreviewAdjust, &request, sizeof(request),
		B_RELATIVE_TIMEOUT, 100000);
	PreviewAdjustmentResult result;
	int32 code = 0;
	if (status == B_OK) {
		ssize_t size = read_port_etc(request.replies, &code, &result, sizeof(result),
			B_RELATIVE_TIMEOUT, 500000);
		status = size < 0 ? (status_t)size
			: size == sizeof(result) && code == kPreviewAdjusted ? result.status : B_BAD_DATA;
		if (status == B_OK) changed = result.changed;
	}
	delete_port(request.replies);
	return status;
}

#ifdef DISPLAYCOLOR_TRACE
static void DiagnosticTrace(const char* event, int32 pipe, uint32 flags,
	uint32 plane, uint32 bottom, int32 changed, status_t status)
{
	FILE* file = fopen("/tmp/displaycolor-package-validation/startup-trace.log", "a");
	if (!file) return;
	fprintf(file, "%lld %s pipe=%d flags=0x%x plane=0x%08x bottom=0x%08x changed=%d status=%d\n",
		(long long)system_time(), event, pipe, flags, plane, bottom, changed, status);
	fclose(file);
}
#else
static void DiagnosticTrace(const char*, int32, uint32, uint32, uint32, int32, status_t)
{
}
#endif

static uint32 Tone(int index, int contrast, int gamma, int filter = 0)
{
	double x = (index / 255.0 - 0.5) * (contrast / 100.0) + 0.5;
	x = pow(std::clamp(x, 0.0, 1.0), 100.0 / gamma);
	uint32 v = (uint32)lround(x * 255.0);
	uint32 green = (uint32)lround(v * (1.0 - 0.20 * filter / 100.0));
	uint32 blue = (uint32)lround(v * (1.0 - 0.65 * filter / 100.0));
	return (v << 16) | (green << 8) | blue;
}

static bool ParseClock(const char* text, int& minutes)
{
	if (!text || strlen(text) != 5 || text[2] != ':') return false;
	for (int index : {0, 1, 3, 4})
		if (text[index] < '0' || text[index] > '9') return false;
	int hours = (text[0] - '0') * 10 + text[1] - '0';
	int minute = (text[3] - '0') * 10 + text[4] - '0';
	if (hours > 23 || minute > 59) return false;
	minutes = hours * 60 + minute;
	return true;
}

static BString FormatClock(int minutes)
{
	minutes = std::clamp(minutes, 0, 1439);
	char text[6];
	snprintf(text, sizeof(text), "%02d:%02d", minutes / 60, minutes % 60);
	return BString(text);
}

static bool ScheduledAt(int minute, int start, int end)
{
	if (start == end) return false;
	return start < end ? minute >= start && minute < end
		: minute >= start || minute < end;
}

struct BlueLightSettings {
	bool enabled = false, scheduled = false;
	int intensity = 40, start = 20 * 60, end = 7 * 60;
	int EffectiveIntensity(int minute) const
	{
		return enabled && (!scheduled || ScheduledAt(minute, start, end))
			? intensity : 0;
	}
};

static double SaturationCoefficient(int row, int column, int saturation)
{
	const double luminance[] = {0.2126, 0.7152, 0.0722};
	double amount = saturation / 100.0;
	return (1.0 - amount) * luminance[column] + (row == column ? amount : 0.0);
}

static uint16 EncodeCoefficient(double value)
{
	if (value == 0.0) return 0;
	double magnitude = std::min(std::abs(value), 3.999);
	int exponent, fractional;
	if (magnitude < 0.125) { exponent = 3; fractional = 12; }
	else if (magnitude < 0.25) { exponent = 2; fractional = 11; }
	else if (magnitude < 0.5) { exponent = 1; fractional = 10; }
	else if (magnitude < 1.0) { exponent = 0; fractional = 9; }
	else if (magnitude < 2.0) { exponent = 7; fractional = 8; }
	else { exponent = 6; fractional = 7; }
	uint32 mantissa = std::min((uint32)lround(std::ldexp(magnitude, fractional)), 511u);
	return (value < 0.0 ? 0x8000 : 0) | (exponent << 12) | (mantissa << 3);
}

static void SaturationRegisters(int red, int green, int blue, uint32 registers[6])
{
	const int saturation[] = {red, green, blue};
	for (int row = 0; row < 3; row++) {
		registers[row * 2] = (uint32)EncodeCoefficient(
			SaturationCoefficient(row, 0, saturation[row])) << 16;
		registers[row * 2] |= EncodeCoefficient(
			SaturationCoefficient(row, 1, saturation[row]));
		registers[row * 2 + 1] = (uint32)EncodeCoefficient(
			SaturationCoefficient(row, 2, saturation[row])) << 16;
	}
}

static uint32 ColorPlaneControl(uint32 current, bool saturation, bool bypassPlaneGamma)
{
	return (current & ~0x00800000u) | 0x40000000
		| (saturation ? 0x00800000 : 0)
		| (saturation && bypassPlaneGamma ? 0x00002000 : 0);
}

struct HardwareSnapshot {
	struct Pipe {
		bool active = false;
		uint32 plane = 0, bottom = 0, gamma = 0, mode = 0;
		uint32 palette[256] = {}, coefficients[6] = {};
		uint32 pre[3] = {}, post[3] = {};
	};
	Pipe pipes[3];
};

class Hardware {
	int fDevice = -1;
	area_id fSharedArea = -1, fRegistersArea = -1;
	intel_shared_info* fInfo = nullptr;
	volatile uint32* fRegisters = nullptr;
	bool fPlaneGammaKnown[3] = {};
	bool fCSCReady[3] = {};
	uint32 fOriginalPlaneGamma[3] = {};
#ifdef DISPLAYCOLOR_TESTING
	uint32* fTestWriteCounts = nullptr;
	struct lock* fTestLock = nullptr;
	void (*fTestAccess)(void*, uint32, bool) = nullptr;
	void* fTestContext = nullptr;
#endif
	struct lock& AccelerantLock()
	{
#ifdef DISPLAYCOLOR_TESTING
		if (fTestLock) return *fTestLock;
#endif
		return fInfo->accelerant_lock;
	}
	uint32 Read(uint32 offset)
	{
#ifdef DISPLAYCOLOR_TESTING
		if (fTestAccess) fTestAccess(fTestContext, offset, false);
#endif
		return fRegisters[offset / 4];
	}
	void Write(uint32 offset, uint32 value)
	{
#ifdef DISPLAYCOLOR_TESTING
		if (fTestWriteCounts) fTestWriteCounts[offset / 4]++;
#endif
		fRegisters[offset / 4] = value;
#ifdef DISPLAYCOLOR_TESTING
		if (fTestAccess) fTestAccess(fTestContext, offset, true);
#endif
		// CSC coefficient/offset reads disarm a pending matrix update.
		// Flush posted writes through pipe control instead of the CSC data.
		uint32 flush = offset >= 0x49000 && offset < 0x49300
			? 0x70008 + ((offset - 0x49000) / 0x100) * 0x1000 : offset;
		(void)Read(flush);
	}
	status_t WaitForFrames(uint32 index)
	{
		// PIPE_FRMCOUNT_G4X: allow buffered color state to latch before
		// reading any CSC data. Two boundaries also cover a commit at vblank.
		uint32 pipe = index * 0x1000;
		uint32 first = Read(0x70040 + pipe);
		bigtime_t deadline = system_time() + 100000;
		do {
			if (!(Read(0x70008 + pipe) & 0x80000000)
				|| !(Read(0x70180 + pipe) & 0x80000000)) return B_WOULD_BLOCK;
			if ((uint32)(Read(0x70040 + pipe) - first) >= 2) return B_OK;
			snooze(500);
		} while (system_time() < deadline);
		return B_TIMED_OUT;
	}
public:
#ifdef DISPLAYCOLOR_TESTING
	void AttachTestRegisters(struct lock* testLock, volatile uint32* registers,
		uint32* writeCounts, void (*access)(void*, uint32, bool) = nullptr,
		void* context = nullptr)
	{
		fTestLock = testLock;
		fRegisters = registers;
		fTestWriteCounts = writeCounts;
		fTestAccess = access;
		fTestContext = context;
	}
#endif
	~Hardware()
	{
		if (fRegistersArea >= 0) delete_area(fRegistersArea);
		if (fSharedArea >= 0) delete_area(fSharedArea);
		if (fDevice >= 0) close(fDevice);
	}
	status_t Init()
	{
		DIR* dir = opendir("/dev/graphics");
		if (!dir) return B_ENTRY_NOT_FOUND;
		while (dirent* entry = readdir(dir)) {
			if (strncmp(entry->d_name, "intel_extreme_", 14)) continue;
			BString path("/dev/graphics/"); path << entry->d_name;
			fDevice = open(path.String(), O_RDWR);
			if (fDevice >= 0) break;
		}
		closedir(dir);
		if (fDevice < 0) return B_NOT_SUPPORTED;
		intel_get_private_data data = {INTEL_PRIVATE_DATA_MAGIC, -1};
		if (ioctl(fDevice, INTEL_GET_PRIVATE_DATA, &data, sizeof(data)))
			return B_ERROR;
		fSharedArea = clone_area("Display Color shared info", (void**)&fInfo,
			B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA, data.shared_info_area);
		if (fSharedArea < 0) return fSharedArea;
		// These register definitions are specific to Skylake and Kaby Lake.
		if (!fInfo->device_type.InGroup(INTEL_GROUP_SKY)
			&& !fInfo->device_type.InGroup(INTEL_GROUP_KBY))
			return B_NOT_SUPPORTED;
		area_info area;
		if (get_area_info(fInfo->registers_area, &area) != B_OK
			|| area.size < 0x73000) return B_BAD_DATA;
		fRegistersArea = clone_area("Display Color registers", (void**)&fRegisters,
			B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA, fInfo->registers_area);
		return fRegistersArea < 0 ? fRegistersArea : B_OK;
	}
	status_t Capture(HardwareSnapshot& snapshot)
	{
		if (!fRegisters) return B_NOT_SUPPORTED;
		Autolock lock(AccelerantLock());
		if (!lock.IsLocked()) return B_ERROR;
		int active = 0;
		for (uint32 index = 0; index < 3; index++) {
			auto& saved = snapshot.pipes[index];
			uint32 pipe = index * 0x1000;
			saved.active = (Read(0x70008 + pipe) & 0x80000000)
				&& (Read(0x70180 + pipe) & 0x80000000);
			if (!saved.active) continue;
			active++;
			saved.plane = Read(0x70180 + pipe);
			if (saved.plane & 0x00800000) {
				status_t status = WaitForFrames(index);
				if (status != B_OK) return status;
			}
			saved.bottom = Read(0x70034 + pipe);
			saved.gamma = Read(0x4a480 + index * 0x800);
			uint32 csc = 0x49010 + index * 0x100;
			saved.mode = Read(csc + 0x18);
			for (int entry = 0; entry < 256; entry++)
				saved.palette[entry] = Read(0x4a000 + index * 0x800 + entry * 4);
			for (int entry = 0; entry < 6; entry++)
				saved.coefficients[entry] = Read(csc + entry * 4);
			for (int entry = 0; entry < 3; entry++) {
				saved.pre[entry] = Read(csc + 0x20 + entry * 4);
				saved.post[entry] = Read(csc + 0x30 + entry * 4);
			}
		}
		return active ? B_OK : B_WOULD_BLOCK;
	}
	status_t Restore(const HardwareSnapshot& snapshot)
	{
		if (!fRegisters) return B_NOT_SUPPORTED;
		Autolock lock(AccelerantLock());
		if (!lock.IsLocked()) return B_ERROR;
		status_t result = B_OK;
		for (uint32 index = 0; index < 3; index++) {
			const auto& saved = snapshot.pipes[index];
			if (!saved.active) continue;
			uint32 pipe = index * 0x1000, plane = 0x70180 + pipe;
			if (!(Read(0x70008 + pipe) & 0x80000000)
				|| !(Read(plane) & 0x80000000)) {
				result = B_BUSY;
				continue;
			}
			uint32 csc = 0x49010 + index * 0x100;
			status_t latched = WaitForFrames(index);
			if (latched != B_OK) return latched;
			bool same = (Read(plane) & 0x40802000u) == (saved.plane & 0x40802000u)
				&& (Read(0x70034 + pipe) & 0xc0000000u) == (saved.bottom & 0xc0000000u)
				&& Read(0x4a480 + index * 0x800) == saved.gamma
				&& Read(csc + 0x18) == saved.mode;
			for (int entry = 0; entry < 256 && same; entry++)
				same = Read(0x4a000 + index * 0x800 + entry * 4) == saved.palette[entry];
			for (int entry = 0; entry < 6 && same; entry++)
				same = Read(csc + entry * 4) == saved.coefficients[entry];
			for (int entry = 0; entry < 3 && same; entry++)
				same = Read(csc + 0x20 + entry * 4) == saved.pre[entry]
					&& Read(csc + 0x30 + entry * 4) == saved.post[entry];
			if (same) continue;
			Write(plane, Read(plane) & ~0x40802000u);
			Write(0x70034 + pipe, Read(0x70034 + pipe) & ~0xc0000000u);
			Write(0x7019c + pipe, Read(0x7019c + pipe));
			for (int entry = 0; entry < 256; entry++)
				Write(0x4a000 + index * 0x800 + entry * 4, saved.palette[entry]);
			for (int entry = 0; entry < 6; entry++)
				Write(csc + entry * 4, saved.coefficients[entry]);
			for (int entry = 0; entry < 3; entry++) {
				Write(csc + 0x20 + entry * 4, saved.pre[entry]);
				Write(csc + 0x30 + entry * 4, saved.post[entry]);
			}
			Write(0x4a480 + index * 0x800, saved.gamma);
			Write(0x70034 + pipe, (Read(0x70034 + pipe) & ~0xc0000000u)
				| (saved.bottom & 0xc0000000u));
			Write(plane, (Read(plane) & ~0x40802000u) | (saved.plane & 0x40802000u));
			Write(0x7019c + pipe, Read(0x7019c + pipe));
			Write(csc + 0x18, saved.mode);
			latched = WaitForFrames(index);
			if (latched != B_OK) return latched;
			for (int entry = 0; entry < 256; entry++)
				if (Read(0x4a000 + index * 0x800 + entry * 4) != saved.palette[entry])
					result = B_IO_ERROR;
			if ((Read(plane) & 0x40802000u) != (saved.plane & 0x40802000u)
				|| (Read(0x70034 + pipe) & 0xc0000000u) != (saved.bottom & 0xc0000000u))
				result = B_IO_ERROR;
		}
		return result;
	}
	bool PanelSelfRefreshEnabled()
	{
		return fRegisters && ((Read(0x6f800) | Read(0x6f900)) & 0x80000000);
	}
	status_t Apply(int contrast, int gamma, int red = 100, int green = 100,
		int blue = 100, int filter = 0, int* changed = nullptr, bool forceCSC = false,
		bool bypassPlaneGamma = true)
	{
		if (!fRegisters) return B_NOT_SUPPORTED;
		Autolock lock(AccelerantLock());
		if (!lock.IsLocked()) return B_ERROR;
		bool saturation = forceCSC || red != 100 || green != 100 || blue != 100;
		if (saturation && PanelSelfRefreshEnabled()) return B_NOT_SUPPORTED;
		uint32 coefficients[6];
		SaturationRegisters(red, green, blue, coefficients);
		int active = 0;
		for (uint32 p = 0; p < 3; p++) {
			uint32 pipe = p * 0x1000, palette = 0x4a000 + p * 0x800;
			uint32 plane = 0x70180 + pipe;
			uint32 planeBefore = Read(plane);
			if (!(Read(0x70008 + pipe) & 0x80000000)
				|| !(planeBefore & 0x80000000)) continue;
			active++;
			if (!fCSCReady[p] && (saturation || (planeBefore & 0x00800000))) {
				status_t latched = WaitForFrames(p);
				if (latched != B_OK) return latched;
				fCSCReady[p] = true;
			}
			if (!fPlaneGammaKnown[p]) {
				fOriginalPlaneGamma[p] = planeBefore & 0x00002000;
				fPlaneGammaKnown[p] = true;
			}
			uint32 desiredPlane = ColorPlaneControl(planeBefore, saturation, bypassPlaneGamma);
			if (!saturation)
				desiredPlane = (desiredPlane & ~0x00002000u) | fOriginalPlaneGamma[p];
			uint32 bottomBefore = Read(0x70034 + pipe);
			uint32 desiredBottom = (bottomBefore & ~0x40000000u) | 0x80000000
				| (saturation ? 0x40000000 : 0);
			uint32 gammaMode = Read(0x4a480 + p * 0x800);
			bool gammaModeMismatch = (gammaMode & 3) != 0;
			bool paletteMismatch = false;
			for (int i = 0; i < 256 && !paletteMismatch; i++)
				paletteMismatch = Read(palette + i * 4) != Tone(i, contrast, gamma, filter);
			uint32 csc = 0x49010 + p * 0x100;
			bool cscMismatch = bool(planeBefore & 0x00800000) != saturation
				|| bool(bottomBefore & 0x40000000) != saturation
				|| (planeBefore & 0x00002000) != (desiredPlane & 0x00002000);
			if (saturation && bypassPlaneGamma)
				cscMismatch |= !(planeBefore & 0x00002000);
			if (saturation) {
				cscMismatch |= Read(csc + 0x18) != 2;
				for (int index = 0; index < 6; index++)
					cscMismatch |= Read(csc + index * 4) != coefficients[index];
				for (uint32 offset : {0x20u, 0x24u, 0x28u, 0x30u, 0x34u, 0x38u})
					cscMismatch |= Read(csc + offset) != 0;
			}
			bool planeMismatch = planeBefore != desiredPlane;
			bool bottomMismatch = bottomBefore != desiredBottom;
			uint32 mismatchFlags = (paletteMismatch ? 1 : 0)
				| (gammaModeMismatch ? 2 : 0) | (cscMismatch ? 4 : 0)
				| (planeMismatch ? 8 : 0) | (bottomMismatch ? 16 : 0);
			DiagnosticTrace("pipe", p, mismatchFlags, planeBefore, bottomBefore, 0, B_OK);
			if (!paletteMismatch && !gammaModeMismatch && !cscMismatch
				&& !planeMismatch && !bottomMismatch) continue;
			if (saturation && cscMismatch) {
				status_t scanning = WaitForFrames(p);
				if (scanning != B_OK) return scanning;
			}
			if (paletteMismatch) {
				for (int i = 0; i < 256; i++) {
					uint32 value = Tone(i, contrast, gamma, filter);
					if (Read(palette + i * 4) != value)
						Write(palette + i * 4, value);
				}
			}
			if (gammaModeMismatch)
				Write(0x4a480 + p * 0x800, gammaMode & ~3u);
			if (saturation && cscMismatch) {
				for (int index = 0; index < 6; index++)
					Write(csc + index * 4, coefficients[index]);
				for (uint32 offset : {0x20u, 0x24u, 0x28u, 0x30u, 0x34u, 0x38u})
					Write(csc + offset, 0);
			}
			if (bottomMismatch) Write(0x70034 + pipe, desiredBottom);
			if (planeMismatch) Write(plane, desiredPlane);
			if (planeMismatch || gammaModeMismatch || bottomMismatch)
				Write(0x7019c + pipe, Read(0x7019c + pipe));
			// Verify data while disarmed, then arm CSC last. Reading the
			// coefficients after CSC_MODE but before vblank cancels latching.
			if (saturation && cscMismatch) {
				for (int index = 0; index < 6; index++)
					if (Read(csc + index * 4) != coefficients[index]) return B_IO_ERROR;
				for (uint32 offset : {0x20u, 0x24u, 0x28u, 0x30u, 0x34u, 0x38u})
					if (Read(csc + offset) != 0) return B_IO_ERROR;
				Write(csc + 0x18, 2);
				fCSCReady[p] = false;
				status_t latched = WaitForFrames(p);
				if (latched != B_OK) return latched;
				fCSCReady[p] = true;
			}
			if (changed) (*changed)++;
			for (int i = 0; i < 256; i++)
				if (Read(palette + i * 4) != Tone(i, contrast, gamma, filter))
					return B_IO_ERROR;
			if (!(Read(plane) & 0x40000000)
				|| (Read(0x4a480 + p * 0x800) & 3)) return B_IO_ERROR;
			if (bool(Read(plane) & 0x00800000) != saturation
				|| bool(Read(0x70034 + pipe) & 0x40000000) != saturation)
				return B_IO_ERROR;
			if ((Read(plane) & 0x40802000u) != (desiredPlane & 0x40802000u)
				|| (Read(0x70034 + pipe) & 0xc0000000u) != (desiredBottom & 0xc0000000u))
				return B_IO_ERROR;
			if (saturation && bypassPlaneGamma && !(Read(plane) & 0x00002000))
				return B_IO_ERROR;
			if (saturation) {
				if (Read(csc + 0x18) != 2) return B_IO_ERROR;
				for (int index = 0; index < 6; index++)
					if (Read(csc + index * 4) != coefficients[index]) return B_IO_ERROR;
				for (uint32 offset : {0x20u, 0x24u, 0x28u, 0x30u, 0x34u, 0x38u})
					if (Read(csc + offset) != 0) return B_IO_ERROR;
			}
		}
		return active ? B_OK : B_WOULD_BLOCK;
	}
};

#ifdef DISPLAYCOLOR_TESTING
static const char* gTestSettingsPath = nullptr;
#endif
static BPath SettingsPath()
{
#ifdef DISPLAYCOLOR_TESTING
	if (gTestSettingsPath) return BPath(gTestSettingsPath);
#endif
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("Display Color");
	return path;
}
static BPath StartupPath()
{
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("boot/launch/Display Color");
	return path;
}
static status_t SetStartup(bool enabled)
{
	BPath path = StartupPath();
	if (!enabled) return unlink(path.Path()) == 0 || errno == ENOENT ? B_OK : B_ERROR;
	app_info info;
	status_t status = be_app->GetAppInfo(&info);
	if (status != B_OK) return status;
	BEntry application(&info.ref, true);
	BPath helper;
	status = application.GetPath(&helper);
	if (status == B_OK) status = helper.GetParent(&helper);
	if (status == B_OK) status = helper.Append("DisplayColorStartup");
	if (status != B_OK) return status;
	if (access(helper.Path(), X_OK) != 0) return B_ENTRY_NOT_FOUND;
	BPath parent; path.GetParent(&parent);
	status = create_directory(parent.Path(), 0755);
	if (status != B_OK) return status;
	struct stat st;
	if (lstat(path.Path(), &st) == 0) {
		char target[1024];
		ssize_t n = readlink(path.Path(), target, sizeof(target) - 1);
		if (n < 0) return B_FILE_EXISTS;
		target[n] = 0;
		return strcmp(target, helper.Path()) == 0
			? B_OK : B_FILE_EXISTS;
	}
	return symlink(helper.Path(), path.Path()) == 0
		? B_OK : B_ERROR;
}

class ToneSlider : public BSlider {
	mutable char fText[32];
	bool fGamma;
public:
	ToneSlider(const char* label, int low, int high, bool gamma)
		: BSlider(label, label, new BMessage(kChange), low, high,
			B_HORIZONTAL), fGamma(gamma)
	{
		SetModificationMessage(new BMessage(kChange));
		SetKeyIncrementValue(1);
		SetExplicitMinSize(BSize(420, B_SIZE_UNSET));
		SetLimitLabels(gamma ? "0.50" : (low == 0 ? "0%" : "50%"),
			gamma ? "3.00" : (high == 100 ? "100%" : "200%"));
	}
	const char* UpdateText() const override
	{
		if (fGamma) snprintf(fText, sizeof(fText), "%.2f", Value() / 100.0);
		else snprintf(fText, sizeof(fText), "%d%%", (int)Value());
		return fText;
	}
};

class Ramp : public BView {
public:
	Ramp() : BView("Grayscale", B_WILL_DRAW)
	{
		SetExplicitMinSize(BSize(420, 64));
	}
	void Draw(BRect) override
	{
		BRect r = Bounds();
		for (int i = 0; i < 256; i++) {
			SetHighColor(i, i, i);
			FillRect(BRect(r.left + r.Width() * i / 256.0, r.top,
				r.left + r.Width() * (i + 1) / 256.0, r.top + 30));
		}
		for (int i = 0; i < 17; i++) {
			int v = i * 255 / 16;
			SetHighColor(v, v, v);
			FillRect(BRect(r.Width() * i / 17.0, r.top + 34,
				r.Width() * (i + 1) / 17.0, r.bottom));
		}
	}
};

class ColorReference : public BView {
public:
	ColorReference() : BView("Color reference", B_WILL_DRAW)
	{
		SetExplicitMinSize(BSize(420, 48));
	}
	void Draw(BRect) override
	{
		const rgb_color colors[] = {
			{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255},
			{0, 255, 255, 255}, {255, 0, 255, 255}, {255, 255, 0, 255},
			{200, 80, 80, 255}, {80, 200, 80, 255}, {80, 80, 200, 255},
			{80, 200, 200, 255}, {200, 80, 200, 255}, {200, 200, 80, 255}
		};
		BRect bounds = Bounds();
		for (int index = 0; index < 12; index++) {
			SetHighColor(colors[index]);
			float width = (bounds.Width() + 1) / 6;
			float height = (bounds.Height() + 1) / 2;
			FillRect(BRect(bounds.left + (index % 6) * width,
				bounds.top + (index / 6) * height,
				bounds.left + (index % 6 + 1) * width - 1,
				bounds.top + (index / 6 + 1) * height - 1));
		}
	}
};

class ColorApp;
class ColorWindow : public BWindow {
public:
	bool exiting = false;
	ToneSlider *contrast, *gamma, *redSaturation, *greenSaturation, *blueSaturation;
	BCheckBox* startup;
	BCheckBox *filterEnabled, *filterScheduled;
	ToneSlider* filterIntensity;
	BTextControl *filterStart, *filterEnd;
	BStringView* filterStatus;
	BStringView* status;
	ColorWindow(int c, int g, int red, int green, int blue,
		const BlueLightSettings& filter)
		: BWindow(BRect(100, 100, 650, 650),
		"Display Color", B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE)
	{
		contrast = new ToneSlider("Contrast", 50, 200, false);
		gamma = new ToneSlider("Gamma", 50, 300, true);
		contrast->SetValue(c); gamma->SetValue(g);
		redSaturation = new ToneSlider("Red saturation", 0, 200, false);
		greenSaturation = new ToneSlider("Green saturation", 0, 200, false);
		blueSaturation = new ToneSlider("Blue saturation", 0, 200, false);
		redSaturation->SetValue(red);
		greenSaturation->SetValue(green);
		blueSaturation->SetValue(blue);
		filterEnabled = new BCheckBox("filter_enabled", "Enable blue light filter",
			new BMessage(kChange));
		filterEnabled->SetValue(filter.enabled);
		filterIntensity = new ToneSlider("Filter intensity", 0, 100, false);
		filterIntensity->SetValue(filter.intensity);
		filterScheduled = new BCheckBox("filter_schedule", "Turn on and off automatically",
			new BMessage(kChange));
		filterScheduled->SetValue(filter.scheduled);
		filterStart = new BTextControl("filter_start", "On at (HH:MM):",
			FormatClock(filter.start).String(), new BMessage(kChange));
		filterEnd = new BTextControl("filter_end", "Off at (HH:MM):",
			FormatClock(filter.end).String(), new BMessage(kChange));
		filterStatus = new BStringView("filter_status", "Blue light filter is off.");
		startup = new BCheckBox("startup", "Start in background at login",
			new BMessage(kStartup));
		startup->SetValue(access(StartupPath().Path(), F_OK) == 0);
		status = new BStringView("status", "Applying display settings…");
		BView* displayControls = new BView("Display", 0);
		BLayoutBuilder::Group<>(displayControls, B_VERTICAL, 8)
			.SetInsets(12)
			.Add(new BStringView("intro", "Adjust contrast, gamma and RGB saturation for the display."))
			.Add(contrast).Add(gamma)
			.Add(redSaturation).Add(greenSaturation).Add(blueSaturation)
			.Add(new BStringView("neutral", "RGB saturation: 100% is neutral; 0% moves a channel to gray."))
			.Add(new ColorReference())
			.Add(new Ramp());
		BView* filterControls = new BView("Blue light filter", 0);
		BLayoutBuilder::Group<>(filterControls, B_VERTICAL, 12)
			.SetInsets(12)
			.Add(filterEnabled).Add(filterIntensity)
			.Add(new BStringView("warmth", "Higher intensity reduces more blue and a little green."))
			.Add(filterScheduled)
			.AddGroup(B_HORIZONTAL, 12)
				.Add(filterStart).Add(filterEnd)
			.End()
			.Add(new BStringView("clock", "Uses the local 24-hour clock; overnight schedules are supported."))
			.Add(new BStringView("running", "Keep the app running; enable login startup for automatic filtering."))
			.Add(filterStatus).AddGlue();
		BTabView* controls = new BTabView("controls");
		controls->AddTab(displayControls);
		controls->AddTab(filterControls);
		BLayoutBuilder::Group<>(this, B_VERTICAL, 12)
			.SetInsets(16)
			.Add(controls)
			.Add(startup).Add(status)
			.AddGroup(B_HORIZONTAL, 8)
				.Add(new BButton("reset", "Defaults", new BMessage(kReset)))
				.AddGlue()
				.Add(new BButton("exit", "Quit", new BMessage(kExit)))
				.Add(new BButton("hide", "Run in background", new BMessage(kHide)))
			.End();
		contrast->SetTarget(be_app); gamma->SetTarget(be_app);
		redSaturation->SetTarget(be_app);
		greenSaturation->SetTarget(be_app);
		blueSaturation->SetTarget(be_app);
		filterEnabled->SetTarget(be_app);
		filterIntensity->SetTarget(be_app);
		filterScheduled->SetTarget(be_app);
		filterStart->SetTarget(be_app);
		filterEnd->SetTarget(be_app);
		startup->SetTarget(be_app);
		FindView("reset")->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		for (const char* name : {"reset", "exit", "hide"})
			dynamic_cast<BButton*>(FindView(name))->SetTarget(be_app);
		CenterOnScreen();
	}
	bool QuitRequested() override
	{
		if (exiting) return true;
		if (!IsHidden()) Hide();
		return false;
	}
};

class ColorApp : public BApplication {
#ifdef DISPLAYCOLOR_TESTING
	friend int PreviewSettingsSelfTest();
#endif
	Hardware fHardware;
	status_t fHardwareStatus = B_ERROR;
	BMessageRunner* fRunner = nullptr;
	ColorWindow* fWindow = nullptr;
	int fContrast = 100, fGamma = 100;
	int fRedSaturation = 100, fGreenSaturation = 100, fBlueSaturation = 100;
	BlueLightSettings fFilter;
	bool fBackground = false, fReady = false, fInputError = false, fPreview = false;
	port_id fPreviewCommands = -1;
	int32 fPreviewToken = 0;
	status_t fSaveStatus = B_OK;
	status_t Save()
	{
		if (fPreview) return B_OK;
		BMessage data;
		data.AddInt32("contrast", fContrast);
		data.AddInt32("gamma", fGamma);
		data.AddInt32("saturation_red", fRedSaturation);
		data.AddInt32("saturation_green", fGreenSaturation);
		data.AddInt32("saturation_blue", fBlueSaturation);
		data.AddBool("filter_enabled", fFilter.enabled);
		data.AddBool("filter_schedule", fFilter.scheduled);
		data.AddInt32("filter_intensity", fFilter.intensity);
		data.AddInt32("filter_start", fFilter.start);
		data.AddInt32("filter_end", fFilter.end);
		BPath path = SettingsPath();
		BString temp(path.Path()); temp << ".tmp";
		BFile file(temp.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t result = file.InitCheck();
		if (result == B_OK) result = data.Flatten(&file);
		if (result == B_OK) result = file.Sync();
		if (result == B_OK && rename(temp.String(), path.Path()) != 0)
			result = B_IO_ERROR;
		if (result != B_OK) unlink(temp.String());
		return result;
	}
	void Apply()
	{
		time_t now = time(nullptr);
		struct tm local;
		int minute = localtime_r(&now, &local) ? local.tm_hour * 60 + local.tm_min : -1;
		int intensity = minute >= 0 ? fFilter.EffectiveIntensity(minute)
			: (fFilter.enabled && !fFilter.scheduled ? fFilter.intensity : 0);
		int changed = 0;
		status_t result = fHardwareStatus;
		if (result == B_OK) {
			if (fPreview) {
				PreviewAdjustment request;
				request.token = fPreviewToken;
				request.contrast = fContrast; request.gamma = fGamma;
				request.red = fRedSaturation; request.green = fGreenSaturation;
				request.blue = fBlueSaturation; request.filter = intensity;
				result = ApplyThroughGuard(fPreviewCommands, request, changed);
			} else result = fHardware.Apply(fContrast, fGamma, fRedSaturation,
				fGreenSaturation, fBlueSaturation, intensity, &changed);
		}
		DiagnosticTrace("apply", -1, intensity, 0, 0, changed, result);
		if (!fWindow || !fWindow->Lock()) return;
		const char* text;
		if (fInputError) text = "Use valid HH:MM times with different on/off times.";
		else if (fSaveStatus != B_OK) text = "Could not save settings. Check your settings folder.";
		else if (result == B_OK) text = fPreview
			? "Temporary preview. The independent guard will restore the display."
			: "Settings saved. Display adjustment is active.";
		else if (result == B_WOULD_BLOCK) text = "Settings saved. Waiting for the display to wake.";
		else if (result == B_NOT_SUPPORTED && fHardwareStatus == B_OK)
			text = "RGB saturation is unavailable while panel self-refresh is enabled.";
		else if (result == B_NOT_SUPPORTED) text = "Unsupported graphics driver. No display changes applied.";
		else text = "Could not apply display settings.";
		fWindow->status->SetText(text);
		bool supported = fHardwareStatus == B_OK;
		fWindow->contrast->SetEnabled(supported);
		fWindow->gamma->SetEnabled(supported);
		fWindow->redSaturation->SetEnabled(supported);
		fWindow->greenSaturation->SetEnabled(supported);
		fWindow->blueSaturation->SetEnabled(supported);
		fWindow->filterEnabled->SetEnabled(supported);
		fWindow->filterIntensity->SetEnabled(supported);
		fWindow->filterScheduled->SetEnabled(supported);
		fWindow->filterStart->SetEnabled(supported);
		fWindow->filterEnd->SetEnabled(supported);
		BString filterText;
		if (!fFilter.enabled) filterText = "Blue light filter is disabled.";
		else if (intensity > 0) filterText.SetToFormat("Blue light filter is on at %d%% intensity.", intensity);
		else filterText = "Blue light filter is off.";
		if (fFilter.enabled && fFilter.scheduled) {
			filterText << " Schedule: " << FormatClock(fFilter.start)
				<< " to " << FormatClock(fFilter.end) << ".";
		}
		fWindow->filterStatus->SetText(filterText.String());
		fWindow->Unlock();
	}
	void ShowWindow()
	{
		if (fWindow->Lock()) {
			if (fWindow->IsHidden()) fWindow->Show();
			fWindow->Minimize(false); fWindow->Activate();
			fWindow->Unlock();
		}
	}
public:
	ColorApp() : BApplication(kSignature)
	{
		BFile file(SettingsPath().Path(), B_READ_ONLY);
		BMessage data;
		if (data.Unflatten(&file) == B_OK) {
			int32 value;
			if (data.FindInt32("contrast", &value) == B_OK)
				fContrast = std::clamp((int)value, 50, 200);
			if (data.FindInt32("gamma", &value) == B_OK)
				fGamma = std::clamp((int)value, 50, 300);
			if (data.FindInt32("saturation_red", &value) == B_OK)
				fRedSaturation = std::clamp((int)value, 0, 200);
			if (data.FindInt32("saturation_green", &value) == B_OK)
				fGreenSaturation = std::clamp((int)value, 0, 200);
			if (data.FindInt32("saturation_blue", &value) == B_OK)
				fBlueSaturation = std::clamp((int)value, 0, 200);
			data.FindBool("filter_enabled", &fFilter.enabled);
			data.FindBool("filter_schedule", &fFilter.scheduled);
			if (data.FindInt32("filter_intensity", &value) == B_OK)
				fFilter.intensity = std::clamp((int)value, 0, 100);
			if (data.FindInt32("filter_start", &value) == B_OK && value >= 0 && value < 1440)
				fFilter.start = value;
			if (data.FindInt32("filter_end", &value) == B_OK && value >= 0 && value < 1440)
				fFilter.end = value;
			if (fFilter.start == fFilter.end) fFilter.scheduled = false;
		}
	}
	~ColorApp() { delete fRunner; }
	bool QuitRequested() override
	{
		if (fWindow && fWindow->Lock()) {
			fWindow->exiting = true;
			fWindow->Unlock();
		}
		return true;
	}
	void ArgvReceived(int32 argc, char** argv) override
	{
		bool background = false;
		for (int i = 1; i < argc; i++)
			if (!strcmp(argv[i], "--background")) background = true;
			else if (!strcmp(argv[i], "--preview") && !fReady) fPreview = true;
			else if (!strcmp(argv[i], "--preview-port") && !fReady && i + 1 < argc)
				fPreviewCommands = atoi(argv[++i]);
			else if (!strcmp(argv[i], "--preview-token") && !fReady && i + 1 < argc)
				fPreviewToken = atoi(argv[++i]);
		if (!fReady) fBackground = background;
		else if (!background) ShowWindow();
	}
	void RefsReceived(BMessage*) override { if (fReady) ShowWindow(); }
	void AppActivated(bool active) override
	{
		if (active && fReady && !fBackground) ShowWindow();
	}
	void ReadyToRun() override
	{
		// The preview frontend never maps hardware or owns the driver lock.
		// Its independent guard applies requests and owns all rollback state.
		fHardwareStatus = fPreview
			? (fPreviewCommands >= 0 ? B_OK : B_NOT_SUPPORTED) : fHardware.Init();
		DiagnosticTrace("ready", -1, 0, 0, 0, 0, fHardwareStatus);
		fWindow = new ColorWindow(fContrast, fGamma, fRedSaturation,
			fGreenSaturation, fBlueSaturation, fFilter);
		if (fPreview) {
			fWindow->SetTitle("Display Color — temporary guarded preview");
			fWindow->startup->SetEnabled(false);
		}
		fReady = true;
		BMessage tick(kTick);
		fRunner = new BMessageRunner(BMessenger(this), &tick, 2000000);
		Apply();
		if (fBackground) {
			// A hidden window still needs its looper running.
			fWindow->Run();
		} else fWindow->Show();
	}
	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case B_SILENT_RELAUNCH: ShowWindow(); break;
			case kChange:
			case kReset:
				if (fWindow->Lock()) {
					if (message->what == kReset) {
						fWindow->contrast->SetValue(100);
						fWindow->gamma->SetValue(100);
						fWindow->redSaturation->SetValue(100);
						fWindow->greenSaturation->SetValue(100);
						fWindow->blueSaturation->SetValue(100);
						fWindow->filterEnabled->SetValue(B_CONTROL_OFF);
						fWindow->filterScheduled->SetValue(B_CONTROL_OFF);
						fWindow->filterIntensity->SetValue(40);
						fWindow->filterStart->SetText("20:00");
						fWindow->filterEnd->SetText("07:00");
					}
					int start, end;
					if (!ParseClock(fWindow->filterStart->Text(), start)
						|| !ParseClock(fWindow->filterEnd->Text(), end)
						|| (fWindow->filterScheduled->Value() == B_CONTROL_ON && start == end)) {
						fInputError = true;
						fWindow->status->SetText("Use valid HH:MM times with different on/off times.");
						fWindow->Unlock();
						break;
					}
					fInputError = false;
					fContrast = fWindow->contrast->Value();
					fGamma = fWindow->gamma->Value();
					fRedSaturation = fWindow->redSaturation->Value();
					fGreenSaturation = fWindow->greenSaturation->Value();
					fBlueSaturation = fWindow->blueSaturation->Value();
					fFilter.enabled = fWindow->filterEnabled->Value() == B_CONTROL_ON;
					fFilter.scheduled = fWindow->filterScheduled->Value() == B_CONTROL_ON;
					fFilter.intensity = fWindow->filterIntensity->Value();
					fFilter.start = start;
					fFilter.end = end;
					fWindow->Unlock();
				}
				fSaveStatus = Save(); Apply(); break;
			case kStartup:
				if (fPreview) break;
				if (fWindow->Lock()) {
					bool enabled = fWindow->startup->Value() == B_CONTROL_ON;
					status_t result = SetStartup(enabled);
					if (result != B_OK) {
						fWindow->startup->SetValue(!enabled);
						fWindow->status->SetText("Could not update the startup shortcut.");
					}
					fWindow->Unlock();
				}
				break;
			case kHide:
				if (fWindow->Lock()) {
					if (!fWindow->IsHidden()) fWindow->Hide();
					fWindow->Unlock();
				}
				break;
			case kTick:
				DiagnosticTrace("tick", -1, 0, 0, 0, 0, B_OK);
				fBackground = false;
				Apply();
				break;
			case kExit: PostMessage(B_QUIT_REQUESTED); break;
			default: BApplication::MessageReceived(message);
		}
	}
};

static int SelfTest()
{
	for (int c : {50, 100, 200}) for (int g : {50, 100, 300}) {
		uint32 previous = 0;
		for (int i = 0; i < 256; i++) {
			uint32 v = Tone(i, c, g);
			if (v < previous || v > 0xffffff
				|| (v & 255) != ((v >> 8) & 255)
				|| (v & 255) != ((v >> 16) & 255)) return 1;
			previous = v;
		}
	}
	for (int i = 0; i < 256; i++)
		if (Tone(i, 100, 100) != (uint32)i * 0x010101) return 2;
	if (Tone(64, 100, 200) <= Tone(64, 100, 100)) return 3;
	if (Tone(64, 150, 100) >= Tone(64, 100, 100)) return 4;
	if (EncodeCoefficient(1.0) != 0x7800 || EncodeCoefficient(-1.0) != 0xf800
		|| EncodeCoefficient(0.0) != 0 || EncodeCoefficient(0.5) != 0x0800)
		return 5;
	for (int amount : {0, 50, 100, 150, 200}) {
		for (int row = 0; row < 3; row++) {
			double sum = 0;
			for (int column = 0; column < 3; column++) {
				double value = SaturationCoefficient(row, column, amount);
				sum += value;
				if (amount == 100 && value != (row == column ? 1.0 : 0.0))
					return 6;
				if (amount == 0 && fabs(value
					- SaturationCoefficient(0, column, 0)) > 1e-12) return 7;
			}
			if (fabs(sum - 1.0) > 1e-12) return 8;
		}
	}
	for (int amount = 0; amount <= 200; amount++) {
		for (int row = 0; row < 3; row++) {
			for (int column = 0; column < 3; column++) {
				double value = SaturationCoefficient(row, column, amount);
				uint16 encoded = EncodeCoefficient(value);
				const int fractional[] = {9, 10, 11, 12, 0, 0, 7, 8};
				double decoded = std::ldexp((encoded >> 3) & 511,
					-fractional[(encoded >> 12) & 7]);
				if (encoded & 0x8000) decoded = -decoded;
				if (fabs(decoded - value) > 0.002) return 21;
			}
		}
	}
	if (ColorPlaneControl(0xc4000000, true, true) != 0xc4802000
		|| ColorPlaneControl(0xc4000000, false, true) != 0xc4000000)
		return 22;
	uint32 neutral[6];
	SaturationRegisters(100, 100, 100, neutral);
	if (neutral[0] != 0x78000000 || neutral[1] != 0
		|| neutral[2] != 0x00007800 || neutral[3] != 0
		|| neutral[4] != 0 || neutral[5] != 0x78000000) return 9;
	for (int channel = 0; channel < 3; channel++) {
		int saturation[] = {100, 100, 100};
		saturation[channel] = 150;
		uint32 registers[6];
		SaturationRegisters(saturation[0], saturation[1], saturation[2], registers);
		for (int row = 0; row < 3; row++) {
			bool same = registers[row * 2] == neutral[row * 2]
				&& registers[row * 2 + 1] == neutral[row * 2 + 1];
			if (same == (row == channel)) return 10;
		}
	}
	for (int intensity : {0, 25, 50, 100}) {
		uint32 previous[3] = {};
		for (int index = 0; index < 256; index++) {
			uint32 color = Tone(index, 100, 100, intensity);
			uint32 channels[] = {color >> 16, (color >> 8) & 255, color & 255};
			if (channels[0] < channels[1] || channels[1] < channels[2]) return 11;
			for (int channel = 0; channel < 3; channel++) {
				if (channels[channel] < previous[channel]) return 12;
				previous[channel] = channels[channel];
			}
		}
	}
	if (Tone(255, 100, 100, 100) != 0xffcc59) return 13;
	int minutes;
	if (!ParseClock("00:00", minutes) || minutes != 0
		|| !ParseClock("23:59", minutes) || minutes != 1439) return 14;
	for (const char* invalid : {"24:00", "12:60", "8:00", "12:3x", "12:30x", "-1:00"})
		if (ParseClock(invalid, minutes)) return 15;
	for (int minute = 0; minute < 1440; minute++) {
		if (ScheduledAt(minute, 1200, 420) != (minute >= 1200 || minute < 420)
			|| ScheduledAt(minute, 480, 1020) != (minute >= 480 && minute < 1020)
			|| ScheduledAt(minute, 600, 600)) return 16;
		if (!ParseClock(FormatClock(minute).String(), minutes) || minutes != minute)
			return 17;
	}
	BlueLightSettings filter;
	if (filter.EffectiveIntensity(0) != 0) return 18;
	filter.enabled = true;
	if (filter.EffectiveIntensity(720) != 40) return 19;
	filter.scheduled = true;
	if (filter.EffectiveIntensity(720) != 0 || filter.EffectiveIntensity(1200) != 40
		|| filter.EffectiveIntensity(419) != 40 || filter.EffectiveIntensity(420) != 0)
		return 20;
	puts("PASS: contrast/gamma, independent RGB saturation, CSC encoding, blue-light intensity and schedules.");
	return 0;
}
int main(int argc, char** argv)
{
	if (argc > 1 && !strcmp(argv[1], "--self-test")) return SelfTest();
	if (argc > 1 && !strcmp(argv[1], "--verify-hardware")) {
		Hardware hardware;
		status_t result = hardware.Init();
		int changed = 0;
		if (result == B_OK) result = hardware.Apply(100, 100, 100, 100, 100, 0, &changed);
		if (result == B_OK) {
			int second = 0;
			result = hardware.Apply(100, 100, 100, 100, 100, 0, &second);
			if (second) result = B_ERROR;
		}
		printf("Hardware neutral curve and register readback: %s (%d); updated %d pipe(s)\n",
			result == B_OK ? "PASS" : "FAIL", result, changed);
		return result == B_OK ? 0 : 1;
	}
	ColorApp app;
	if (app.InitCheck() != B_OK) return 1;
	app.Run();
	return 0;
}
