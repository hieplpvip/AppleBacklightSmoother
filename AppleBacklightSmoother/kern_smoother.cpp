//
//  kern_smoother.cpp
//  AppleBacklightSmoother
//
//  Copyright © 2020 Le Bao Hiep. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_version.hpp>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOLocks.h>

#include "kern_smoother.hpp"

OSDefineMetaClassAndStructors(PRODUCT_NAME, IOService)

PRODUCT_NAME *ADDPR(selfInstance) = nullptr;

IOService *PRODUCT_NAME::probe(IOService *provider, SInt32 *score) {
	ADDPR(selfInstance) = this;
	setProperty("Copyright", "Copyright © 2020 Le Bao Hiep. All rights reserved.");
	setProperty("VersionInfo", kextVersion);
	auto service = IOService::probe(provider, score);
	return ADDPR(startSuccess) ? service : nullptr;
}

bool PRODUCT_NAME::start(IOService *provider) {
	ADDPR(selfInstance) = this;
	if (!IOService::start(provider)) {
		SYSLOG("start", "failed to start the parent");
		return false;
	}

	AppleBacklightSmootherNS::lockSmooth = IORecursiveLockAlloc();
	if (!AppleBacklightSmootherNS::lockSmooth) {
		SYSLOG("start", "failed to allocate lock");
		return false;
	}

	AppleBacklightSmootherNS::workLoop = getWorkLoop();
	if (!AppleBacklightSmootherNS::workLoop) {
		SYSLOG("start", "failed to get workloop");
		return false;
	}

	AppleBacklightSmootherNS::smoothTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, nullptr, &PRODUCT_NAME::dischargeQueue));
	if (!AppleBacklightSmootherNS::smoothTimer || AppleBacklightSmootherNS::workLoop->addEventSource(AppleBacklightSmootherNS::smoothTimer) != kIOReturnSuccess) {
		SYSLOG("start", "failed to create smooth timer");
		return false;
	}

	return ADDPR(startSuccess);
}

void PRODUCT_NAME::stop(IOService *provider) {
	ADDPR(selfInstance) = nullptr;
	if (AppleBacklightSmootherNS::smoothTimer) {
		AppleBacklightSmootherNS::workLoop->removeEventSource(AppleBacklightSmootherNS::smoothTimer);
		OSSafeReleaseNULL(AppleBacklightSmootherNS::smoothTimer);
	}
	if (AppleBacklightSmootherNS::workLoop) {
		OSSafeReleaseNULL(AppleBacklightSmootherNS::workLoop);
	}
	IOService::stop(provider);
}

void PRODUCT_NAME::dischargeQueue() {
	IORecursiveLockLock(AppleBacklightSmootherNS::lockSmooth);
	while (!AppleBacklightSmootherNS::backlightQueue.isEmpty()) {
#define IMIN(A, B) ((A < B) ? (A) : (B))
#define IMAX(A, B) ((A > B) ? (A) : (B))
		auto pair = AppleBacklightSmootherNS::backlightQueue.fetch();
		if (pair.third > IMIN(AppleBacklightSmootherNS::currentBacklightValue, AppleBacklightSmootherNS::lastRequestedBacklightValue) &&
			pair.third < IMAX(AppleBacklightSmootherNS::currentBacklightValue, AppleBacklightSmootherNS::lastRequestedBacklightValue)) {
			AppleBacklightSmootherNS::orgWriteRegister32(pair.first, AppleBacklightSmootherNS::backlightDutyRegister, pair.second);
			AppleBacklightSmootherNS::currentBacklightValue = pair.third;
			DBGLOG("smoother", "dischargeQueue set backlight register 0x%x to %0x%x", AppleBacklightSmootherNS::backlightDutyRegister, pair.second);
#ifdef DEBUG
			if (ADDPR(selfInstance)) {
				ADDPR(selfInstance)->setProperty("Current Backlight Value", AppleBacklightSmootherNS::currentBacklightValue);
			}
#endif
			break;
		}
#undef IMIN
#undef IMAX
	}
	if (!AppleBacklightSmootherNS::backlightQueue.isEmpty()) {
		AppleBacklightSmootherNS::smoothTimer->setTimeoutMS(AppleBacklightSmootherNS::DELAYMS);
	}
	IORecursiveLockUnlock(AppleBacklightSmootherNS::lockSmooth);
}

void AppleBacklightSmootherNS::init_plugin() {
	workLoop = nullptr;
	smoothTimer = nullptr;
	lockSmooth = nullptr;
	currentFramebuffer = nullptr;
	currentFramebufferOpt = nullptr;
	orgReadRegister32 = nullptr;
	orgWriteRegister32 = nullptr;
	backlightValueAssigned = false;
	lastRequestedBacklightValue = 0;
	currentBacklightValue = 0;
	targetBacklightFrequency = 0;
	targetPwmControl = 0;
	driverBacklightFrequency = 0;
	backlightDutyRegister = 0;
	tableGenerated = false;

#ifdef DEBUG
	loggedFrequency = false;
#endif

	auto &bdi = BaseDeviceInfo::get();
	auto generation = bdi.cpuGeneration;
	auto family = bdi.cpuFamily;
	auto model = bdi.cpuModel;
	switch (generation) {
		case CPUInfo::CpuGeneration::Penryn:
		case CPUInfo::CpuGeneration::Nehalem:
			// Do not warn about legacy processors
			break;
		case CPUInfo::CpuGeneration::Westmere:
			currentFramebuffer = &kextIntelHDFb;
			break;
		case CPUInfo::CpuGeneration::SandyBridge:
			currentFramebuffer = &kextIntelSNBFb;
			break;
		case CPUInfo::CpuGeneration::IvyBridge:
			currentFramebuffer = &kextIntelCapriFb;
			break;
		case CPUInfo::CpuGeneration::Haswell:
			currentFramebuffer = &kextIntelAzulFb;
			break;
		case CPUInfo::CpuGeneration::Broadwell:
			currentFramebuffer = &kextIntelBDWFb;
			break;
		case CPUInfo::CpuGeneration::Skylake:
			currentFramebuffer = &kextIntelSKLFb;
			break;
		case CPUInfo::CpuGeneration::KabyLake:
			currentFramebuffer = &kextIntelKBLFb;
			break;
		case CPUInfo::CpuGeneration::CoffeeLake:
			currentFramebuffer = &kextIntelCFLFb;
			currentFramebufferOpt = &kextIntelKBLFb;
			break;
		case CPUInfo::CpuGeneration::CannonLake:
			currentFramebuffer = &kextIntelCNLFb;
			break;
		case CPUInfo::CpuGeneration::IceLake:
			currentFramebuffer = &kextIntelICLLPFb;
			currentFramebufferOpt = &kextIntelICLHPFb;
			break;
		case CPUInfo::CpuGeneration::CometLake:
			currentFramebuffer = &kextIntelCFLFb;
			currentFramebufferOpt = &kextIntelKBLFb;
			break;
		default:
			SYSLOG("smoother", "found an unsupported processor 0x%X:0x%X, please report this!", family, model);
			break;
	}

	if (currentFramebuffer) {
		lilu.onKextLoadForce(currentFramebuffer);
	}

	if (currentFramebufferOpt) {
		lilu.onKextLoadForce(currentFramebufferOpt);
	}

	lilu.onKextLoadForce(nullptr, 0,
	[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
		processKext(patcher, index, address, size);
	}, nullptr);
}

void AppleBacklightSmootherNS::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if ((currentFramebuffer && currentFramebuffer->loadIndex == index) ||
		(currentFramebufferOpt && currentFramebufferOpt->loadIndex == index)) {
		// Find actual framebuffer used
		auto realFramebuffer = (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;

		// Find original ReadRegister32
		orgReadRegister32 = patcher.solveSymbol<decltype(orgReadRegister32)>(index, "__ZN31AppleIntelFramebufferController14ReadRegister32Em", address, size);
		if (!orgReadRegister32) {
			SYSLOG("smoother", "Failed to find ReadRegister32");
			patcher.clearError();
			return;
		}

		// Find original WriteRegister32
		orgWriteRegister32 = patcher.solveSymbol<decltype(orgWriteRegister32)>(index, "__ZN31AppleIntelFramebufferController15WriteRegister32Emj", address, size);
		if (!orgReadRegister32) {
			SYSLOG("smoother", "Failed to find WriteRegister32");
			patcher.clearError();
			return;
		}

		// Determine which function to route to
		auto cpuGeneration = BaseDeviceInfo::get().cpuGeneration;
		decltype(wrapIvyWriteRegister32) *wrapWriteRegister32;
		if (cpuGeneration <= CPUInfo::CpuGeneration::IvyBridge) {
			wrapWriteRegister32 = wrapIvyWriteRegister32;
			backlightDutyRegister = BLC_PWM_CPU_CTL;
		} else if (cpuGeneration <= CPUInfo::CpuGeneration::KabyLake) {
			wrapWriteRegister32 = wrapHswWriteRegister32;
			backlightDutyRegister = BXT_BLC_PWM_FREQ1;
		} else {
			// Get CPU stepping to determine if it's Kaby Lake-R of Coffee Lake
			uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
			asm volatile ("xchgq %%rbx, %q1\n"
						  "cpuid\n"
						  "xchgq %%rbx, %q1"
						  : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
						  : "0" (1), "2" (0));

			uint32_t stepping = eax & 0xf;
			if (stepping == 0xa) { // Kaby Lake-R
				if (realFramebuffer == &kextIntelCFLFb) {
					wrapWriteRegister32 = wrapKblFakeWriteRegister32;
				} else {
					wrapWriteRegister32 = wrapHswWriteRegister32;
				}
				backlightDutyRegister = BXT_BLC_PWM_FREQ1;
			} else { // Coffee Lake
				if (realFramebuffer == &kextIntelCFLFb) {
					wrapWriteRegister32 = wrapCflRealWriteRegister32;
				} else {
					wrapWriteRegister32 = wrapCflFakeWriteRegister32;
				}
				backlightDutyRegister = BXT_BLC_PWM_DUTY1;
			}
		}

		// Route WriteRegister32
		patcher.eraseCoverageInstPrefix(reinterpret_cast<mach_vm_address_t>(orgWriteRegister32));
		orgWriteRegister32 = reinterpret_cast<decltype(orgWriteRegister32)>(patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(orgWriteRegister32), reinterpret_cast<mach_vm_address_t>(wrapWriteRegister32), true));

		if (!orgWriteRegister32) {
			SYSLOG("smoother", "Failed to route WriteRegister32");
			patcher.clearError();
			return;
		}

		DBGLOG("smoother", "Successfully routed hwSetBacklight");
	}
}

void AppleBacklightSmootherNS::generateTables() {
	// DUTY = a * STEP ^ 2 + START_VALUE
	double a = static_cast<double>(targetBacklightFrequency - START_VALUE) / static_cast<double>(STEPS * STEPS);
	for (int i = 0; i < STEPS; i++) {
		dutyTables[i] = static_cast<uint32_t>(a * i * i + START_VALUE + 0.5f); // round to nearest integer
	}
	tableGenerated = true;
}

int AppleBacklightSmootherNS::lowerBound(uint32_t *data, int from, int to, int value) {
	int result = to--, mid;
	while (from <= to) {
		mid = (from + to) >> 1;
		if (data[mid] >= value) {
			result = mid;
			to = mid - 1;
		} else {
			from = mid + 1;
		}
	}
	return result;
}

int AppleBacklightSmootherNS::upperBound(uint32_t *data, int from, int to, int value) {
	int result = to--, mid;
	while (from <= to) {
		mid = (from + to) >> 1;
		if (data[mid] > value) {
			result = mid;
			to = mid - 1;
		} else {
			from = mid + 1;
		}
	}
	return result;
}

void AppleBacklightSmootherNS::pushQueue(void *that, uint32_t value, uint32_t mask) {
#ifdef DEBUG
	if (!loggedFrequency && ADDPR(selfInstance)) {
		loggedFrequency = true;
		ADDPR(selfInstance)->setProperty("Target Backlight Frequency", OSNumber::withNumber(targetBacklightFrequency, 32));
		ADDPR(selfInstance)->setProperty("Target PWM Control", OSNumber::withNumber(targetPwmControl, 32));
		ADDPR(selfInstance)->setProperty("Driver Backlight Frequency", OSNumber::withNumber(driverBacklightFrequency, 32));

		OSArray *dutyTablesArray = OSArray::withCapacity(STEPS);
		for (int i = 0; i < STEPS; i++) {
			dutyTablesArray->setObject(OSNumber::withNumber(dutyTables[i], 32));
		}
		ADDPR(selfInstance)->setProperty("Duty Tables", dutyTablesArray);
	}
#endif

	if (lastRequestedBacklightValue == value) {
		return;
	}

	if (!tableGenerated) {
		orgWriteRegister32(that, backlightDutyRegister, mask | value);
		currentBacklightValue = value;
		return;
	}

	IORecursiveLockLock(lockSmooth);
	bool isQueueEmpty = backlightQueue.isEmpty();

	if (lastRequestedBacklightValue < value) {
		int from = upperBound(dutyTables, 0, STEPS, lastRequestedBacklightValue);
		int to = lowerBound(dutyTables, 0, STEPS, value) - 1;

		if (from < STEPS && to < STEPS) {
			for (int i = from; i <= to; i++) {
				backlightQueue.push(SimpleTriple<void *, uint32_t, uint32_t>(that, mask | dutyTables[i], dutyTables[i]));
			}
		}

		backlightQueue.push(SimpleTriple<void *, uint32_t, uint32_t>(that, mask | value, value));
	} else { // lastRequestedBacklightValue > value
		int from = lowerBound(dutyTables, 0, STEPS, lastRequestedBacklightValue) - 1;
		int to = upperBound(dutyTables, 0, STEPS, value);

		if (from < STEPS && to < STEPS) {
			for (int i = from; i >= to; i--) {
				backlightQueue.push(SimpleTriple<void *, uint32_t, uint32_t>(that, mask | dutyTables[i], dutyTables[i]));
			}
		}

		backlightQueue.push(SimpleTriple<void *, uint32_t, uint32_t>(that, mask | value, value));
	}

	lastRequestedBacklightValue = value;
#ifdef DEBUG
	if (ADDPR(selfInstance)) {
		ADDPR(selfInstance)->setProperty("Last Requested Backlight Value", lastRequestedBacklightValue);
	}
#endif

	if (isQueueEmpty) {
		smoothTimer->setTimeoutMS(DELAYMS);
	}

	IORecursiveLockUnlock(lockSmooth);
}

void AppleBacklightSmootherNS::wrapIvyWriteRegister32(void *that, uint32_t reg, uint32_t value) {
	if (reg == BXT_BLC_PWM_FREQ1) {
		if (value && value != driverBacklightFrequency) {
			DBGLOG("smoother", "wrapIvyWriteRegister32: driver requested BXT_BLC_PWM_FREQ1 = 0x%x", value);
			driverBacklightFrequency = value;
		}

		if (targetBacklightFrequency == 0) {
			// Save the hardware PWM frequency as initially set up by the system firmware.
			// We'll need this to restore later after system sleep.
			targetBacklightFrequency = orgReadRegister32(that, BXT_BLC_PWM_FREQ1);
			DBGLOG("smoother", "wrapIvyWriteRegister32: system initialized BXT_BLC_PWM_FREQ1 = 0x%x", targetBacklightFrequency);

			if (targetBacklightFrequency == 0) {
				// This should not happen with correctly written bootloader code, but in case it does, let's use a failsafe default value.
				targetBacklightFrequency = FallbackTargetBacklightFrequency;
				SYSLOG("smoother", "wrapIvyWriteRegister32: system initialized BXT_BLC_PWM_FREQ1 is ZERO");
			}

			generateTables();
		}

		if (value) {
			// Nonzero writes to this register need to use the original system value.
			// Yet the driver can safely write zero to this register as part of system sleep.
			value = targetBacklightFrequency;
		}
	} else if (reg == BLC_PWM_CPU_CTL) {
		if (driverBacklightFrequency && targetBacklightFrequency) {
			// Translate the PWM duty cycle between the driver scale value and the HW scale value
			uint32_t rescaledValue = static_cast<uint32_t>((static_cast<uint64_t>(value) * static_cast<uint64_t>(targetBacklightFrequency)) / static_cast<uint64_t>(driverBacklightFrequency));
			DBGLOG("smoother", "wrapIvyWriteRegister32: write BLC_PWM_CPU_CTL 0x%x/0x%x, rescaled to 0x%x/0x%x", value, driverBacklightFrequency, rescaledValue, targetBacklightFrequency);

			if (lockSmooth && backlightValueAssigned) {
				pushQueue(that, rescaledValue);
				return;
			}

			value = rescaledValue;
			backlightValueAssigned = true;
			lastRequestedBacklightValue = currentBacklightValue = rescaledValue;
		} else {
			// This should never happen, but in case it does we should log it at the very least.
			SYSLOG("smoother", "wrapIvyWriteRegister32: write BLC_PWM_CPU_CTL has zero frequency driver (%d) target (%d)", driverBacklightFrequency, targetBacklightFrequency);
		}
	}

	orgWriteRegister32(that, reg, value);
}

void AppleBacklightSmootherNS::wrapHswWriteRegister32(void *that, uint32_t reg, uint32_t value) {
	if (reg == BXT_BLC_PWM_FREQ1) {
		// BXT_BLC_PWM_FREQ1 controls the backlight intensity.
		// High 16 of this register are the denominator (frequency), low 16 are the numerator (duty cycle).

		if (targetBacklightFrequency == 0) {
			// Populate the hardware PWM frequency as initially set up by the system firmware.
			uint32_t org_value = orgReadRegister32(that, BXT_BLC_PWM_FREQ1);
			targetBacklightFrequency = (org_value & 0xffff0000U) >> 16U;
			DBGLOG("smoother", "wrapHswWriteRegister32: system initialized PWM frequency = 0x%x", targetBacklightFrequency);

			if (targetBacklightFrequency == 0) {
				// This should not happen with correctly written bootloader code, but in case it does, let's use a failsafe default value.
				targetBacklightFrequency = FallbackTargetBacklightFrequency;
				SYSLOG("smoother", "wrapHswWriteRegister32: system initialized PWM frequency is ZERO");
			}

			generateTables();
		}

		uint32_t dutyCycle = value & 0xffffU;
		uint32_t frequency = (value & 0xffff0000U) >> 16U;

		if (frequency && frequency != driverBacklightFrequency) {
			DBGLOG("smoother", "wrapHswWriteRegister32: driver requested max frequency = 0x%x", frequency);
			driverBacklightFrequency = frequency;
		}

		uint32_t rescaledValue = frequency == 0 ? 0 : static_cast<uint32_t>((static_cast<uint64_t>(dutyCycle) * static_cast<uint64_t>(targetBacklightFrequency)) / static_cast<uint64_t>(frequency));
		DBGLOG("smoother", "wrapHswWriteRegister32: write BXT_BLC_PWM_FREQ1 0x%x/0x%x, rescaled to 0x%x/0x%x", dutyCycle, driverBacklightFrequency, rescaledValue, targetBacklightFrequency);

		if (frequency) {
			// Nonzero writes to frequency need to use the original system frequency.
			// Yet the driver can safely write zero to this register as part of system sleep.
			frequency = targetBacklightFrequency;
		}

		if (lockSmooth && backlightValueAssigned) {
			pushQueue(that, rescaledValue, (frequency << 16U));
			return;
		}

		// Write the rescaled duty cycle and frequency
		value = (frequency << 16U) | rescaledValue;
		backlightValueAssigned = true;
		lastRequestedBacklightValue = currentBacklightValue = rescaledValue;
	}

	orgWriteRegister32(that, reg, value);
}

void AppleBacklightSmootherNS::wrapKblFakeWriteRegister32(void *that, uint32_t reg, uint32_t value) {
	if (reg == BXT_BLC_PWM_FREQ1) {
		uint32_t frequency = value;

		if (frequency && frequency != driverBacklightFrequency) {
			DBGLOG("smoother", "wrapKblFakeWriteRegister32: driver requested BXT_BLC_PWM_FREQ1 = 0x%x", value);
			driverBacklightFrequency = frequency;
		}

		if (targetBacklightFrequency == 0) {
			// Save the hardware PWM frequency as initially set up by the system firmware.
			// We'll need this to restore later after system sleep.
			targetBacklightFrequency = (orgReadRegister32(that, BXT_BLC_PWM_FREQ1) & 0xffff0000U) >> 16U;
			DBGLOG("smoother", "wrapKblFakeWriteRegister32: system initialized PWM MAX = 0x%x", targetBacklightFrequency);

			if (targetBacklightFrequency == 0) {
				// This should not happen with correctly written bootloader code, but in case it does, let's use a failsafe default value.
				targetBacklightFrequency = FallbackTargetBacklightFrequency;
				SYSLOG("smoother", "wrapKblFakeWriteRegister32: system initialized PWM MAX is ZERO");
			}

			generateTables();
		}

		if (frequency) {
			// Nonzero writes to PWM frequency need to use the original system value.
			// Yet the driver can safely write zero as part of system sleep.
			frequency = targetBacklightFrequency;
		}

		// Read current duty cycle
		uint32_t dutyCycle = orgReadRegister32(that, BXT_BLC_PWM_FREQ1) & 0xffffU;

		value = (frequency << 16U) | dutyCycle;
	} else if (reg == BXT_BLC_PWM_DUTY1) {
		if (driverBacklightFrequency && targetBacklightFrequency) {
			// Translate the PWM duty cycle between the driver scale value and the HW scale value
			uint32_t rescaledValue = static_cast<uint32_t>((static_cast<uint64_t>(value) * static_cast<uint64_t>(targetBacklightFrequency)) / static_cast<uint64_t>(driverBacklightFrequency));
			DBGLOG("smoother", "wrapKblFakeWriteRegister32: write PWM_DUTY1 0x%x/0x%x, rescaled to 0x%x/0x%x", value, driverBacklightFrequency, rescaledValue, targetBacklightFrequency);

			// Read current frequency
			uint32_t frequency = (orgReadRegister32(that, BXT_BLC_PWM_FREQ1) & 0xffff0000U) >> 16U;

			if (lockSmooth && backlightValueAssigned) {
				pushQueue(that, rescaledValue, (frequency << 16U));
				return;
			}

			reg = BXT_BLC_PWM_FREQ1;
			value = (frequency << 16U) | rescaledValue;
			backlightValueAssigned = true;
			lastRequestedBacklightValue = currentBacklightValue = rescaledValue;
		} else {
			// This should never happen, but in case it does we should log it at the very least.
			SYSLOG("smoother", "wrapKblFakeWriteRegister32: write PWM_DUTY1 has zero frequency driver (%d) target (%d)", driverBacklightFrequency, targetBacklightFrequency);
		}
	}

	orgWriteRegister32(that, reg, value);
}

void AppleBacklightSmootherNS::wrapCflRealWriteRegister32(void *that, uint32_t reg, uint32_t value) {
	if (reg == BXT_BLC_PWM_FREQ1) {
		if (value && value != driverBacklightFrequency) {
			DBGLOG("smoother", "wrapCflRealWriteRegister32: driver requested BXT_BLC_PWM_FREQ1 = 0x%x", value);
			driverBacklightFrequency = value;
		}

		if (targetBacklightFrequency == 0) {
			// Save the hardware PWM frequency as initially set up by the system firmware.
			// We'll need this to restore later after system sleep.
			targetBacklightFrequency = orgReadRegister32(that, BXT_BLC_PWM_FREQ1);
			DBGLOG("smoother", "wrapCflRealWriteRegister32: system initialized BXT_BLC_PWM_FREQ1 = 0x%x", targetBacklightFrequency);

			if (targetBacklightFrequency == 0) {
				// This should not happen with correctly written bootloader code, but in case it does, let's use a failsafe default value.
				targetBacklightFrequency = FallbackTargetBacklightFrequency;
				SYSLOG("smoother", "wrapCflRealWriteRegister32: system initialized BXT_BLC_PWM_FREQ1 is ZERO");
			}

			generateTables();
		}

		if (value) {
			// Nonzero writes to this register need to use the original system value.
			// Yet the driver can safely write zero to this register as part of system sleep.
			value = targetBacklightFrequency;
		}
	} else if (reg == BXT_BLC_PWM_DUTY1) {
		if (driverBacklightFrequency && targetBacklightFrequency) {
			// Translate the PWM duty cycle between the driver scale value and the HW scale value
			uint32_t rescaledValue = static_cast<uint32_t>((static_cast<uint64_t>(value) * static_cast<uint64_t>(targetBacklightFrequency)) / static_cast<uint64_t>(driverBacklightFrequency));
			DBGLOG("smoother", "wrapCflRealWriteRegister32: write PWM_DUTY1 0x%x/0x%x, rescaled to 0x%x/0x%x", value, driverBacklightFrequency, rescaledValue, targetBacklightFrequency);

			if (lockSmooth && backlightValueAssigned) {
				pushQueue(that, rescaledValue);
				return;
			}

			value = rescaledValue;
			backlightValueAssigned = true;
			lastRequestedBacklightValue = currentBacklightValue = rescaledValue;
		} else {
			// This should never happen, but in case it does we should log it at the very least.
			SYSLOG("smoother", "wrapCflRealWriteRegister32: write PWM_DUTY1 has zero frequency driver (%d) target (%d)", driverBacklightFrequency, targetBacklightFrequency);
		}
	}

	orgWriteRegister32(that, reg, value);
}

void AppleBacklightSmootherNS::wrapCflFakeWriteRegister32(void *that, uint32_t reg, uint32_t value) {
	if (reg == BXT_BLC_PWM_FREQ1) { // aka BLC_PWM_PCH_CTL2
		if (targetBacklightFrequency == 0) {
			// Populate the hardware PWM frequency as initially set up by the system firmware.
			targetBacklightFrequency = orgReadRegister32(that, BXT_BLC_PWM_FREQ1);
			DBGLOG("smoother", "wrapCflFakeWriteRegister32: system initialized BXT_BLC_PWM_FREQ1 = 0x%x", targetBacklightFrequency);
			DBGLOG("smoother", "wrapCflFakeWriteRegister32: system initialized BXT_BLC_PWM_CTL1 = 0x%x", orgReadRegister32(that, BXT_BLC_PWM_CTL1));

			if (targetBacklightFrequency == 0) {
				// This should not happen with correctly written bootloader code, but in case it does, let's use a failsafe default value.
				targetBacklightFrequency = FallbackTargetBacklightFrequency;
				SYSLOG("smoother", "wrapCflFakeWriteRegister32: system initialized BXT_BLC_PWM_FREQ1 is ZERO");
			}

			generateTables();
		}

		// For the KBL driver, 0xc8254 (BLC_PWM_PCH_CTL2) controls the backlight intensity.
		// High 16 of this write are the denominator (frequency), low 16 are the numerator (duty cycle).
		// Translate this into a write to c8258 (BXT_BLC_PWM_DUTY1) for the CFL hardware, scaled by the system-provided value in c8254 (BXT_BLC_PWM_FREQ1).
		uint16_t frequency = (value & 0xffff0000U) >> 16U;
		uint16_t dutyCycle = value & 0xffffU;

		uint32_t rescaledValue = frequency == 0 ? 0 : static_cast<uint32_t>((static_cast<uint64_t>(dutyCycle) * static_cast<uint64_t>(targetBacklightFrequency)) / static_cast<uint64_t>(frequency));
		DBGLOG("smoother", "wrapCflFakeWriteRegister32: write PWM_DUTY1 0x%x/0x%x, rescaled to 0x%x/0x%x", dutyCycle, frequency, rescaledValue, targetBacklightFrequency);

		// Reset the hardware PWM frequency. Write the original system value if the driver-requested value is nonzero. If the driver requests
		// zero, we allow that, since it's trying to turn off the backlight PWM for sleep.
		orgWriteRegister32(that, BXT_BLC_PWM_FREQ1, frequency ? targetBacklightFrequency : 0);

		if (lockSmooth && backlightValueAssigned) {
			pushQueue(that, rescaledValue);
			return;
		}

		// Finish by writing the duty cycle.
		reg = BXT_BLC_PWM_DUTY1;
		value = rescaledValue;
		backlightValueAssigned = true;
		lastRequestedBacklightValue = currentBacklightValue = rescaledValue;
	} else if (reg == BXT_BLC_PWM_CTL1) {
		if (targetPwmControl == 0) {
			// Save the original hardware PWM control value
			targetPwmControl = orgReadRegister32(that, BXT_BLC_PWM_CTL1);
		}

		DBGLOG("smoother", "wrapCflFakeWriteRegister32: write BXT_BLC_PWM_CTL1 0x%x, previous was 0x%x", value, orgReadRegister32(that, BXT_BLC_PWM_CTL1));

		if (value) {
			// Set the PWM frequency before turning it on to avoid the 3 minute blackout bug
			orgWriteRegister32(that, BXT_BLC_PWM_FREQ1, targetBacklightFrequency);

			// Use the original hardware PWM control value.
			value = targetPwmControl;
		}
	}

	orgWriteRegister32(that, reg, value);
}

static const char *bootargOff[] {
	"-applbklsmoothoff"
};

static const char *bootargDebug[] {
	"-applbklsmoothdbg"
};

static const char *bootargBeta[] {
	"-applbklsmoothbeta"
};

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
	parseModuleVersion(xStringify(MODULE_VERSION)),
	LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery,
	bootargOff,
	arrsize(bootargOff),
	bootargDebug,
	arrsize(bootargDebug),
	bootargBeta,
	arrsize(bootargBeta),
	KernelVersion::MountainLion,
	KernelVersion::BigSur,
	[]() {
		AppleBacklightSmootherNS::init_plugin();
	}
};
