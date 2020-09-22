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

IOReturn AppleBacklightSmootherDummy::wrapHwSetBacklightGated(void *that, uint32_t *backlight) {
	DBGLOG("smoother", "wrapHwSetBacklight called: backlight 0x%x", *backlight);

	if (SmootherData::backlightValueAssigned) {
		uint32_t curBacklight = SmootherData::backlightValue;
		uint32_t newBacklight = *backlight;
		uint32_t diff = (newBacklight > curBacklight) ? (newBacklight - curBacklight) : (curBacklight - newBacklight);
		if (diff > 0x10) {
			uint32_t steps = (diff < 0x40) ? 4 : 16;
			uint32_t delta = diff / steps;
			if (newBacklight > curBacklight) {
				while ((curBacklight += delta) < newBacklight) {
					DBGLOG("smoother", "wrapHwSetBacklight set backlight 0x%x delta %d", curBacklight, delta);
					SmootherData::orgHwSetBacklight(that, curBacklight);
					IOSleep(10); // Wait 5 miliseconds
				}
			} else {
				while (curBacklight > delta && (curBacklight -= delta) > newBacklight) {
					DBGLOG("smoother", "wrapHwSetBacklight set backlight 0x%x step %d", curBacklight, delta);
					SmootherData::orgHwSetBacklight(that, curBacklight);
					IOSleep(10); // Wait 5 miliseconds
				}
			}
		}
	}

	SmootherData::backlightValue = *backlight;
	SmootherData::backlightValueAssigned = true;
	SmootherData::orgHwSetBacklight(that, *backlight);

	return kIOReturnSuccess;
}

IOReturn SmootherData::wrapHwSetBacklight(void *that, uint32_t backlight) {
	if (!triedCmdGate && !SmootherData::cmdGate && ADDPR(selfInstance)) {
		triedCmdGate = true;

		IOWorkLoop* workLoop = ADDPR(selfInstance)->getWorkLoop();
		if (workLoop) {
			SmootherData::cmdGate = IOCommandGate::commandGate(ADDPR(selfInstance));
			if (SmootherData::cmdGate) {
				workLoop->addEventSource(SmootherData::cmdGate);
			}
		}
	}

	if (SmootherData::cmdGate) {
		SmootherData::cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, nullptr, &AppleBacklightSmootherDummy::wrapHwSetBacklightGated), that, (void *)&backlight);
	} else {
		SmootherData::orgHwSetBacklight(that, backlight);
	}

	return kIOReturnSuccess;
}

void SmootherData::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if ((SmootherData::currentFramebuffer && SmootherData::currentFramebuffer->loadIndex == index) ||
		(SmootherData::currentFramebufferOpt && SmootherData::currentFramebufferOpt->loadIndex == index)) {
		// Find actual framebuffer used
		auto realFramebuffer = (SmootherData::currentFramebuffer && SmootherData::currentFramebuffer->loadIndex == index) ? SmootherData::currentFramebuffer : SmootherData::currentFramebufferOpt;

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
		SmootherData::orgHwSetBacklight = reinterpret_cast<decltype(SmootherData::orgHwSetBacklight)>(patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(solvedSymbol), reinterpret_cast<mach_vm_address_t>(SmootherData::wrapHwSetBacklight), true));
		if (!SmootherData::orgHwSetBacklight) {
			SYSLOG("smoother", "Failed to route hwSetBacklight");
			patcher.clearError();
			return;
		}

		DBGLOG("smoother", "Successfully routed hwSetBacklight");
	}
}

void SmootherData::init_plugin() {
	SmootherData::cmdGate = nullptr;
	SmootherData::triedCmdGate = false;
	SmootherData::currentFramebuffer = nullptr;
	SmootherData::currentFramebufferOpt = nullptr;
	SmootherData::orgHwSetBacklight = nullptr;
	SmootherData::backlightValueAssigned = false;
	SmootherData::backlightValue = 0;

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
			SmootherData::currentFramebuffer = &kextIntelSNBFb;
			break;
		case CPUInfo::CpuGeneration::IvyBridge:
			SmootherData::currentFramebuffer = &kextIntelCapriFb;
			break;
		case CPUInfo::CpuGeneration::Haswell:
			SmootherData::currentFramebuffer = &kextIntelAzulFb;
			break;
		case CPUInfo::CpuGeneration::Broadwell:
			SmootherData::currentFramebuffer = &kextIntelBDWFb;
			break;
		case CPUInfo::CpuGeneration::Skylake:
			SmootherData::currentFramebuffer = &kextIntelSKLFb;
			break;
		case CPUInfo::CpuGeneration::KabyLake:
			SmootherData::currentFramebuffer = &kextIntelKBLFb;
			break;
		case CPUInfo::CpuGeneration::CoffeeLake:
			SmootherData::currentFramebuffer = &kextIntelCFLFb;
			SmootherData::currentFramebufferOpt = &kextIntelKBLFb;
			break;
		case CPUInfo::CpuGeneration::CannonLake:
			SmootherData::currentFramebuffer = &kextIntelCNLFb;
			break;
		case CPUInfo::CpuGeneration::IceLake:
			SmootherData::currentFramebuffer = &kextIntelICLLPFb;
			SmootherData::currentFramebufferOpt = &kextIntelICLHPFb;
			break;
		case CPUInfo::CpuGeneration::CometLake:
			SmootherData::currentFramebuffer = &kextIntelCFLFb;
			SmootherData::currentFramebufferOpt = &kextIntelKBLFb;
			break;
		default:
			SYSLOG("smoother", "found an unsupported processor 0x%X:0x%X, please report this!", family, model);
			break;
	}

	if (SmootherData::currentFramebuffer)
		lilu.onKextLoadForce(SmootherData::currentFramebuffer);

	if (SmootherData::currentFramebufferOpt)
		lilu.onKextLoadForce(SmootherData::currentFramebufferOpt);

	lilu.onKextLoadForce(nullptr, 0,
	[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
		SmootherData::processKext(patcher, index, address, size);
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
		SmootherData::init_plugin();
	}
};
