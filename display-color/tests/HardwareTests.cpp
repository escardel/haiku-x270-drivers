#define DISPLAYCOLOR_TESTING
#define main DisplayColorAppMain
#include "../DisplayColor.cpp"
#undef main

#include <numeric>
#include <vector>

// Model the behavior an ordinary register array misses: CSC data reads
// cancel a pending commit; the matrix becomes visible at a frame boundary.
struct MatrixModel {
	std::vector<uint32>& registers;
	bool armed[3] = {};
	uint32 visible[3][6] = {};
	int prematureReads = 0;
	bool advanceFrames = true;
	static void Access(void* context, uint32 offset, bool write)
	{
		auto& model = *static_cast<MatrixModel*>(context);
		for (uint32 pipe = 0; pipe < 3; pipe++) {
			uint32 csc = 0x49010 + pipe * 0x100;
			if (write && offset == csc + 0x18) model.armed[pipe] = true;
			bool data = (offset >= csc && offset < csc + 0x18)
				|| (offset >= csc + 0x20 && offset <= csc + 0x28)
				|| (offset >= csc + 0x30 && offset <= csc + 0x38);
			if (!write && data && model.armed[pipe]) {
				model.prematureReads++;
				model.armed[pipe] = false;
			}
			if (!write && offset == 0x70040 + pipe * 0x1000 && model.advanceFrames) {
				model.registers[offset / 4]++;
				if (model.armed[pipe]) {
					for (int entry = 0; entry < 6; entry++)
						model.visible[pipe][entry] = model.registers[csc / 4 + entry];
					model.armed[pipe] = false;
				}
			}
		}
	}
};

#define REQUIRE(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #condition); \
			return 1; \
		} \
	} while (0)

int main()
{
	const uint32 pipeControl = 0x70008;
	const uint32 plane = 0x70180;
	const uint32 surface = 0x7019c;
	const uint32 bottom = 0x70034;
	const uint32 palette = 0x4a000;
	const uint32 gammaMode = 0x4a480;
	const uint32 cscMode = 0x49028;
	std::vector<uint32> registers(0x73000 / 4);
	std::vector<uint32> writes(registers.size());
	MatrixModel model{registers};
	struct lock testLock;
	REQUIRE(init_lock(&testLock, "Display Color hardware test") == B_OK);
	Hardware hardware;
	hardware.AttachTestRegisters(&testLock, registers.data(), writes.data(),
		MatrixModel::Access, &model);
	registers[pipeControl / 4] = 0x80000000;
	registers[plane / 4] = 0xc4000000;
	registers[bottom / 4] = 0x80000000;
	registers[0x71180 / 4] = 0x04000000;
	for (int index = 0; index < 256; index++)
		registers[palette / 4 + index] = Tone(index, 100, 80);
	auto totalWrites = [&]() {
		return std::accumulate(writes.begin(), writes.end(), uint32(0));
	};
	auto clearWrites = [&]() { std::fill(writes.begin(), writes.end(), 0); };
	HardwareSnapshot original;
	REQUIRE(hardware.Capture(original) == B_OK);
	REQUIRE(hardware.Restore(original) == B_OK && totalWrites() == 0);
	int changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 0 && totalWrites() == 0);
	registers[palette / 4 + 128] ^= 0x00010101;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 1 && totalWrites() == 1);
	REQUIRE(writes[palette / 4 + 128] == 1);
	REQUIRE(writes[plane / 4] == 0 && writes[surface / 4] == 0);
	clearWrites();
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 15, &changed) == B_OK);
	REQUIRE(changed == 1 && totalWrites() > 0);
	REQUIRE(writes[plane / 4] == 0 && writes[surface / 4] == 0);
	REQUIRE(writes[bottom / 4] == 0 && writes[gammaMode / 4] == 0);
	for (int index = 0; index < 256; index++)
		REQUIRE(registers[palette / 4 + index] == Tone(index, 100, 80, 15));
	clearWrites();
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 15, &changed) == B_OK);
	REQUIRE(changed == 0 && totalWrites() == 0);
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 105, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 1 && registers[plane / 4] == 0xc4802000);
	REQUIRE(registers[bottom / 4] == 0xc0000000 && registers[cscMode / 4] == 2);
	REQUIRE(writes[plane / 4] == 1 && writes[surface / 4] == 1);
	uint32 expected[6];
	SaturationRegisters(105, 100, 100, expected);
	for (int entry = 0; entry < 6; entry++)
		REQUIRE(model.visible[0][entry] == expected[entry]);
	REQUIRE(model.prematureReads == 0);
	clearWrites();
	for (int tick = 0; tick < 10; tick++) {
		changed = 0;
		REQUIRE(hardware.Apply(100, 80, 105, 100, 100, 0, &changed) == B_OK);
		REQUIRE(changed == 0 && totalWrites() == 0);
	}
	// Changing RGB while CSC is already enabled must latch the new matrix
	// without repeatedly touching the plane or its surface.
	for (int channel = 0; channel < 3; channel++) {
		int rgb[] = {100, 100, 100};
		rgb[channel] = 50;
		clearWrites();
		changed = 0;
		REQUIRE(hardware.Apply(100, 80, rgb[0], rgb[1], rgb[2], 0, &changed) == B_OK);
		SaturationRegisters(rgb[0], rgb[1], rgb[2], expected);
		for (int entry = 0; entry < 6; entry++)
			REQUIRE(model.visible[0][entry] == expected[entry]);
		REQUIRE(changed == 1 && model.prematureReads == 0);
		REQUIRE(writes[plane / 4] == 0 && writes[surface / 4] == 0);
	}
	REQUIRE(hardware.Apply(100, 80, 105, 100, 100) == B_OK);
	clearWrites();
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 105, 100, 100, 15, &changed) == B_OK);
	REQUIRE(changed == 1 && totalWrites() > 0);
	REQUIRE(writes[plane / 4] == 0 && writes[surface / 4] == 0);
	REQUIRE(writes[bottom / 4] == 0 && writes[cscMode / 4] == 0);
	clearWrites();
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 1 && registers[plane / 4] == 0xc4000000);
	REQUIRE(registers[bottom / 4] == 0x80000000);
	REQUIRE(writes[plane / 4] == 1 && writes[surface / 4] == 1);
	clearWrites();
	registers[plane / 4] &= ~0x40000000u;
	registers[gammaMode / 4] = 3;
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 1 && registers[plane / 4] == 0xc4000000);
	REQUIRE(registers[gammaMode / 4] == 0 && writes[surface / 4] == 1);
	for (int index = 0; index < 256; index++)
		REQUIRE(writes[palette / 4 + index] == 0);
	clearWrites();
	registers[gammaMode / 4] = 3;
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 1 && registers[gammaMode / 4] == 0);
	REQUIRE(writes[plane / 4] == 0 && writes[surface / 4] == 1);
	clearWrites();
	registers[bottom / 4] = 0;
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 100, 100, 100, 0, &changed) == B_OK);
	REQUIRE(changed == 1 && registers[bottom / 4] == 0x80000000);
	REQUIRE(writes[bottom / 4] == 1 && writes[plane / 4] == 0 && writes[surface / 4] == 1);
	clearWrites();
	registers[0x6f800 / 4] = 0x80000000;
	REQUIRE(hardware.Apply(100, 80, 105, 100, 100) == B_NOT_SUPPORTED);
	REQUIRE(totalWrites() == 0);
	REQUIRE(registers[0x71180 / 4] == 0x04000000 && writes[0x71180 / 4] == 0);
	registers[0x6f800 / 4] = 0;
	REQUIRE(hardware.Restore(original) == B_OK);
	HardwareSnapshot restored;
	REQUIRE(hardware.Capture(restored) == B_OK);
	for (int pipe = 0; pipe < 3; pipe++) {
		const auto& before = original.pipes[pipe];
		const auto& after = restored.pipes[pipe];
		REQUIRE(before.active == after.active);
		if (!before.active) continue;
		REQUIRE(before.plane == after.plane && before.bottom == after.bottom);
		REQUIRE(before.gamma == after.gamma && before.mode == after.mode);
		for (int entry = 0; entry < 256; entry++)
			REQUIRE(before.palette[entry] == after.palette[entry]);
		for (int entry = 0; entry < 6; entry++)
			REQUIRE(before.coefficients[entry] == after.coefficients[entry]);
		for (int entry = 0; entry < 3; entry++)
			REQUIRE(before.pre[entry] == after.pre[entry]
				&& before.post[entry] == after.post[entry]);
	}
	clearWrites();
	REQUIRE(hardware.Restore(original) == B_OK && totalWrites() == 0);
	// A stalled scanout must fail before touching matrix or plane state.
	model.advanceFrames = false;
	changed = 0;
	REQUIRE(hardware.Apply(100, 80, 150, 100, 100, 0, &changed) == B_TIMED_OUT);
	REQUIRE(changed == 0 && totalWrites() == 0 && model.prematureReads == 0);
	model.advanceFrames = true;
	registers[pipeControl / 4] = 0;
	changed = 0;
	REQUIRE(hardware.Apply(110, 120, 105, 100, 100, 15, &changed) == B_WOULD_BLOCK);
	REQUIRE(changed == 0 && totalWrites() == 0);
	uninit_lock(&testLock);
	puts("PASS: CSC frame latching and read-disarm protection, independent RGB updates, stable refresh, palette repair, filter, mode reset, snapshot restoration, stalled/sleeping display and PSR safety (simulated registers).");
	return 0;
}
