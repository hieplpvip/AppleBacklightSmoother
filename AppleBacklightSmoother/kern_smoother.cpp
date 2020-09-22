//
//  kern_smoother.cpp
//  AppleBacklightSmoother
//
//  Copyright © 2020 Le Bao Hiep. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <IOKit/IOCommandGate.h>

#include "kern_smoother.hpp"

static const char *pathIntelSNBFb[]   { "/System/Library/Extensions/AppleIntelSNBGraphicsFB.kext/Contents/MacOS/AppleIntelSNBGraphicsFB" };
static const char *pathIntelCapriFb[] { "/System/Library/Extensions/AppleIntelFramebufferCapri.kext/Contents/MacOS/AppleIntelFramebufferCapri" };
static const char *pathIntelAzulFb[]  { "/System/Library/Extensions/AppleIntelFramebufferAzul.kext/Contents/MacOS/AppleIntelFramebufferAzul" };
static const char *pathIntelBDWFb[]   { "/System/Library/Extensions/AppleIntelBDWGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelBDWGraphicsFramebuffer" };
static const char *pathIntelSKLFb[]   { "/System/Library/Extensions/AppleIntelSKLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelSKLGraphicsFramebuffer" };
static const char *pathIntelKBLFb[]   { "/System/Library/Extensions/AppleIntelKBLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelKBLGraphicsFramebuffer" };
static const char *pathIntelCFLFb[]   { "/System/Library/Extensions/AppleIntelCFLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelCFLGraphicsFramebuffer" };
static const char *pathIntelCNLFb[]   { "/System/Library/Extensions/AppleIntelCNLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelCNLGraphicsFramebuffer" };
static const char *pathIntelICLLPFb[] { "/System/Library/Extensions/AppleIntelICLLPGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelICLLPGraphicsFramebuffer" };
static const char *pathIntelICLHPFb[] { "/System/Library/Extensions/AppleIntelICLHPGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelICLHPGraphicsFramebuffer" };

static KernelPatcher::KextInfo kextIntelSNBFb   { "com.apple.driver.AppleIntelSNBGraphicsFB", pathIntelSNBFb, arrsize(pathIntelSNBFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelCapriFb { "com.apple.driver.AppleIntelFramebufferCapri", pathIntelCapriFb, arrsize(pathIntelCapriFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelAzulFb  { "com.apple.driver.AppleIntelFramebufferAzul", pathIntelAzulFb, arrsize(pathIntelAzulFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelBDWFb   { "com.apple.driver.AppleIntelBDWGraphicsFramebuffer", pathIntelBDWFb, arrsize(pathIntelBDWFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelSKLFb   { "com.apple.driver.AppleIntelSKLGraphicsFramebuffer", pathIntelSKLFb, arrsize(pathIntelSKLFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelKBLFb   { "com.apple.driver.AppleIntelKBLGraphicsFramebuffer", pathIntelKBLFb, arrsize(pathIntelKBLFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelCFLFb   { "com.apple.driver.AppleIntelCFLGraphicsFramebuffer", pathIntelCFLFb, arrsize(pathIntelCFLFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelCNLFb   { "com.apple.driver.AppleIntelCNLGraphicsFramebuffer", pathIntelCNLFb, arrsize(pathIntelCNLFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelICLLPFb { "com.apple.driver.AppleIntelICLLPGraphicsFramebuffer", pathIntelICLLPFb, arrsize(pathIntelICLLPFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextIntelICLHPFb { "com.apple.driver.AppleIntelICLHPGraphicsFramebuffer", pathIntelICLHPFb, arrsize(pathIntelICLHPFb), {}, {}, KernelPatcher::KextInfo::Unloaded };

static AppleBacklightSmootherController *ADDPR(callbackSmoother);
static IOCommandGate* ADDPR(cmdGate);

static KernelPatcher::KextInfo *ADDPR(currentFramebuffer);
static KernelPatcher::KextInfo *ADDPR(currentFramebufferOpt);

static uint32_t (*ADDPR(orgHwSetBacklight))(void *, uint32_t);
static bool ADDPR(backlightValueAssigned);
static uint32_t ADDPR(backlightValue);

#define super IOService
OSDefineMetaClassAndStructors(AppleBacklightSmootherController, IOService)

bool AppleBacklightSmootherController::start(IOService *provider) {
	if (ADDPR(callbackSmoother)) {
		return false;
	}

	if (!super::start(provider)) {
		SYSLOG("smoother", "failed to start super");
		return false;
	}

	ADDPR(callbackSmoother) = this;

	IOWorkLoop* workLoop = getWorkLoop();
	if (!workLoop) {
		return false;
	}

	ADDPR(cmdGate) = IOCommandGate::commandGate(this);
	if (ADDPR(cmdGate)) {
		workLoop->addEventSource(ADDPR(cmdGate));
	}

	return true;
}

EXPORT IOReturn ADDPR(wrapHwSetBacklight)(void *that, uint32_t backlight) {
	if (ADDPR(cmdGate)) {
		ADDPR(cmdGate)->runAction(OSMemberFunctionCast(IOCommandGate::Action, ADDPR(callbackSmoother), &AppleBacklightSmootherController::wrapHwSetBacklightGated), that, (void *)&backlight);
	} else {
		ADDPR(orgHwSetBacklight)(that, backlight);
	}
	return kIOReturnSuccess;
}

IOReturn AppleBacklightSmootherController::wrapHwSetBacklightGated(void *that, uint32_t *backlight) {
	DBGLOG("smoother", "wrapHwSetBacklight called: backlight 0x%x", *backlight);

	if (ADDPR(backlightValueAssigned)) {
		uint32_t curBacklight = ADDPR(backlightValue);
		uint32_t newBacklight = *backlight;
		uint32_t diff = (newBacklight > curBacklight) ? (newBacklight - curBacklight) : (curBacklight - newBacklight);
		if (diff > 0x10) {
			uint32_t steps = (diff < 0x40) ? 4 : 16;
			uint32_t delta = diff / steps;
			if (newBacklight > curBacklight) {
				while ((curBacklight += delta) < newBacklight) {
					DBGLOG("smoother", "wrapHwSetBacklight set backlight 0x%x delta %d", curBacklight, delta);
					ADDPR(orgHwSetBacklight)(that, curBacklight);
					IOSleep(10); // Wait 5 miliseconds
				}
			} else {
				while (curBacklight > delta && (curBacklight -= delta) > newBacklight) {
					DBGLOG("smoother", "wrapHwSetBacklight set backlight 0x%x step %d", curBacklight, delta);
					ADDPR(orgHwSetBacklight)(that, curBacklight);
					IOSleep(10); // Wait 5 miliseconds
				}
			}
		}
	}

	ADDPR(backlightValue) = *backlight;
	ADDPR(backlightValueAssigned) = true;
	ADDPR(orgHwSetBacklight)(that, *backlight);

	return kIOReturnSuccess;
}

void ADDPR(processKext)(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if ((ADDPR(currentFramebuffer) && ADDPR(currentFramebuffer)->loadIndex == index) ||
		(ADDPR(currentFramebufferOpt) && ADDPR(currentFramebufferOpt)->loadIndex == index)) {
		// Find actual framebuffer used
		auto realFramebuffer = (ADDPR(currentFramebuffer) && ADDPR(currentFramebuffer)->loadIndex == index) ? ADDPR(currentFramebuffer) : ADDPR(currentFramebufferOpt);

		// Find hwSetBacklight symbol
		auto hwSetBacklightSym = "";
		if (realFramebuffer == &kextIntelSNBFb) { // Sandy Bridge
			hwSetBacklightSym = ""; //FIXME: find the symbol
		} else if (realFramebuffer == &kextIntelCapriFb) { // Ivy Bridge
			hwSetBacklightSym = "__ZN25AppleIntelCapriController14hwSetBacklightEj";
		} else if (realFramebuffer == &kextIntelAzulFb) { // Haswell
			hwSetBacklightSym = "__ZN24AppleIntelAzulController14hwSetBacklightEj";
		} else if (realFramebuffer == &kextIntelBDWFb) { // Broadwell
			hwSetBacklightSym = "__ZN22AppleIntelFBController14hwSetBacklightEj";
		} else if (realFramebuffer == &kextIntelSKLFb) { // Skylake and newer
			hwSetBacklightSym = "__ZN31AppleIntelFramebufferController14hwSetBacklightEj";
		}

		// Solve hwSetBacklight symbol
		auto solvedSymbol = patcher.solveSymbol(index, hwSetBacklightSym, address, size);
		if (!solvedSymbol) {
			SYSLOG("smoother", "Failed to find hwSetBacklight");
			patcher.clearError();
			return;
		}

		// Route hwSetBacklight to wrapHwSetBacklight
		ADDPR(orgHwSetBacklight) = reinterpret_cast<decltype(ADDPR(orgHwSetBacklight))>(patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(solvedSymbol), reinterpret_cast<mach_vm_address_t>(ADDPR(wrapHwSetBacklight)), true));
		if (!ADDPR(orgHwSetBacklight)) {
			SYSLOG("smoother", "Failed to route hwSetBacklight");
			patcher.clearError();
			return;
		}

		DBGLOG("smoother", "Successfully routed hwSetBacklight");
	}
}

void ADDPR(init_plugin)() {
	ADDPR(callbackSmoother) = nullptr;
	ADDPR(cmdGate) = nullptr;
	ADDPR(orgHwSetBacklight) = nullptr;
	ADDPR(currentFramebuffer) = ADDPR(currentFramebufferOpt) = nullptr;

	ADDPR(backlightValueAssigned) = false;
	ADDPR(backlightValue) = 0;

	auto &bdi = BaseDeviceInfo::get();
	auto generation = bdi.cpuGeneration;
	auto family = bdi.cpuFamily;
	auto model = bdi.cpuModel;
	switch (generation) {
		case CPUInfo::CpuGeneration::Penryn:
		case CPUInfo::CpuGeneration::Nehalem:
		case CPUInfo::CpuGeneration::Westmere:
			// Do not warn about legacy processors
			break;
		case CPUInfo::CpuGeneration::SandyBridge:
			ADDPR(currentFramebuffer) = &kextIntelSNBFb;
			break;
		case CPUInfo::CpuGeneration::IvyBridge:
			ADDPR(currentFramebuffer) = &kextIntelCapriFb;
			break;
		case CPUInfo::CpuGeneration::Haswell:
			ADDPR(currentFramebuffer) = &kextIntelAzulFb;
			break;
		case CPUInfo::CpuGeneration::Broadwell:
			ADDPR(currentFramebuffer) = &kextIntelBDWFb;
			break;
		case CPUInfo::CpuGeneration::Skylake:
			ADDPR(currentFramebuffer) = &kextIntelSKLFb;
			break;
		case CPUInfo::CpuGeneration::KabyLake:
			ADDPR(currentFramebuffer) = &kextIntelKBLFb;
			break;
		case CPUInfo::CpuGeneration::CoffeeLake:
			ADDPR(currentFramebuffer) = &kextIntelCFLFb;
			ADDPR(currentFramebufferOpt) = &kextIntelKBLFb;
			break;
		case CPUInfo::CpuGeneration::CannonLake:
			ADDPR(currentFramebuffer) = &kextIntelCNLFb;
			break;
		case CPUInfo::CpuGeneration::IceLake:
			ADDPR(currentFramebuffer) = &kextIntelICLLPFb;
			ADDPR(currentFramebufferOpt) = &kextIntelICLHPFb;
			break;
		case CPUInfo::CpuGeneration::CometLake:
			ADDPR(currentFramebuffer) = &kextIntelCFLFb;
			ADDPR(currentFramebufferOpt) = &kextIntelKBLFb;
			break;
		default:
			SYSLOG("smoother", "found an unsupported processor 0x%X:0x%X, please report this!", family, model);
			break;
	}

	if (ADDPR(currentFramebuffer))
		lilu.onKextLoadForce(ADDPR(currentFramebuffer));

	if (ADDPR(currentFramebufferOpt))
		lilu.onKextLoadForce(ADDPR(currentFramebufferOpt));

	lilu.onKextLoadForce(nullptr, 0,
	[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
		ADDPR(processKext)(patcher, index, address, size);
	}, nullptr);
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
		ADDPR(init_plugin)();
	}
};
