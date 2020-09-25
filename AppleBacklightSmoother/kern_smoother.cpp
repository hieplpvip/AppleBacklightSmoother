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

static const char kextVersion[] {
#ifdef DEBUG
	'D', 'B', 'G', '-',
#else
	'R', 'E', 'L', '-',
#endif
	xStringify(MODULE_VERSION)[0], xStringify(MODULE_VERSION)[2], xStringify(MODULE_VERSION)[4], '-',
	getBuildYear<0>(), getBuildYear<1>(), getBuildYear<2>(), getBuildYear<3>(), '-',
	getBuildMonth<0>(), getBuildMonth<1>(), '-', getBuildDay<0>(), getBuildDay<1>(), '\0'
};

OSDefineMetaClassAndStructors(PRODUCT_NAME, IOService)

PRODUCT_NAME *ADDPR(selfInstance) = nullptr;

IOService *PRODUCT_NAME::probe(IOService *provider, SInt32 *score) {
	ADDPR(selfInstance) = this;
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

	workLoop = ADDPR(selfInstance)->getWorkLoop();
	if (!workLoop) {
		SYSLOG("start", "failed to get workloop");
		return false;
	}

	cmdGate = IOCommandGate::commandGate(this);
	if (!cmdGate || workLoop->addEventSource(cmdGate) != kIOReturnSuccess) {
		SYSLOG("start", "failed to open command gate");
		return false;
	}

	return ADDPR(startSuccess);
}

void PRODUCT_NAME::stop(IOService *provider) {
	ADDPR(selfInstance) = nullptr;
	if (cmdGate) {
		workLoop->removeEventSource(cmdGate);
		OSSafeReleaseNULL(cmdGate);
	}
	if (workLoop) {
		OSSafeReleaseNULL(workLoop);
	}
	IOService::stop(provider);
}

IOReturn PRODUCT_NAME::wrapHwSetBacklightGated(void *that, uint32_t *backlight) {
	DBGLOG("smoother", "wrapHwSetBacklight called: backlight 0x%x", *backlight);

	if (backlightValueAssigned) {
		uint32_t curBacklight = backlightValue;
		uint32_t newBacklight = *backlight;
		uint32_t diff = (newBacklight > curBacklight) ? (newBacklight - curBacklight) : (curBacklight - newBacklight);
		if (diff > 0x10) {
			uint32_t steps = (diff < 0x40) ? 4 : 16;
			uint32_t delta = diff / steps;
			if (newBacklight > curBacklight) {
				while ((curBacklight += delta) < newBacklight) {
					DBGLOG("smoother", "wrapHwSetBacklight set backlight 0x%x delta %d", curBacklight, delta);
					orgHwSetBacklight(that, curBacklight);
					IOSleep(10); // Wait 5 miliseconds
				}
			} else {
				while (curBacklight > delta && (curBacklight -= delta) > newBacklight) {
					DBGLOG("smoother", "wrapHwSetBacklight set backlight 0x%x step %d", curBacklight, delta);
					orgHwSetBacklight(that, curBacklight);
					IOSleep(10); // Wait 5 miliseconds
				}
			}
		}
	}

	backlightValue = *backlight;
	backlightValueAssigned = true;
	orgHwSetBacklight(that, *backlight);

	return kIOReturnSuccess;
}

IOReturn PRODUCT_NAME::wrapHwSetBacklight(void *that, uint32_t backlight) {
	if (cmdGate) {
		cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, ADDPR(selfInstance), &PRODUCT_NAME::wrapHwSetBacklightGated), that, (void *)&backlight);
	} else {
		orgHwSetBacklight(that, backlight);
	}

	return kIOReturnSuccess;
}

void PRODUCT_NAME::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if ((currentFramebuffer && currentFramebuffer->loadIndex == index) ||
		(currentFramebufferOpt && currentFramebufferOpt->loadIndex == index)) {
		// Find actual framebuffer used
		auto realFramebuffer = (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;

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
		orgHwSetBacklight = reinterpret_cast<decltype(orgHwSetBacklight)>(patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(solvedSymbol), reinterpret_cast<mach_vm_address_t>(wrapHwSetBacklight), true));
		if (!orgHwSetBacklight) {
			SYSLOG("smoother", "Failed to route hwSetBacklight");
			patcher.clearError();
			return;
		}

		DBGLOG("smoother", "Successfully routed hwSetBacklight");
	}
}

void PRODUCT_NAME::init_plugin() {
	workLoop = nullptr;
	cmdGate = nullptr;
	currentFramebuffer = nullptr;
	currentFramebufferOpt = nullptr;
	orgHwSetBacklight = nullptr;
	backlightValueAssigned = false;
	backlightValue = 0;

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

	if (currentFramebuffer)
		lilu.onKextLoadForce(currentFramebuffer);

	if (currentFramebufferOpt)
		lilu.onKextLoadForce(currentFramebufferOpt);

	lilu.onKextLoadForce(nullptr, 0,
	[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
		PRODUCT_NAME::processKext(patcher, index, address, size);
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
		PRODUCT_NAME::init_plugin();
	}
};
