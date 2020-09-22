//
//  kern_smoother.cpp
//  AppleBacklightSmoother
//
//  Copyright © 2020 Le Bao Hiep. All rights reserved.
//

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

Smoother *Smoother::callbackSmoother;

void Smoother::init() {
	callbackSmoother = this;

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
		static_cast<Smoother *>(user)->processKext(patcher, index, address, size);
	}, this);
}

IOReturn Smoother::wrapHwSetBacklight(void *that, uint32_t backlight) {
	callbackSmoother->backlightValueAssigned = true;
	callbackSmoother->backlightValue = backlight;
	DBGLOG("smoother", "wrapHwSetBacklight called: backlight 0x%x", backlight);
	return callbackSmoother->orgHwSetBacklight(that, backlight);
}

void Smoother::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if ((currentFramebuffer && currentFramebuffer->loadIndex == index) ||
		(currentFramebufferOpt && currentFramebufferOpt->loadIndex == index)) {
		auto cpuGeneration = BaseDeviceInfo::get().cpuGeneration;

		// Find actual framebuffer used
		auto realFramebuffer = (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;

		if (cpuGeneration == CPUInfo::CpuGeneration::SandyBridge) {
			gPlatformInformationList = patcher.solveSymbol<void *>(index, "_PlatformInformationList", address, size);
		} else {
			gPlatformInformationList = patcher.solveSymbol<void *>(index, "_gPlatformInformationList", address, size);
		}

		// Find max backlight frequency and hwSetBacklight symbol
		auto hwSetBacklightSym = "";
		if (realFramebuffer == &kextIntelSNBFb) { // Sandy Bridge
			maxBacklightFrequency = static_cast<FramebufferSNB *>(gPlatformInformationList)[0].fBacklightMax;
			hwSetBacklightSym = ""; //FIXME: find the symbol
		} else if (realFramebuffer == &kextIntelCapriFb) { // Ivy Bridge
			maxBacklightFrequency = static_cast<FramebufferIVB *>(gPlatformInformationList)[0].fBacklightMax;
			hwSetBacklightSym = "__ZN25AppleIntelCapriController14hwSetBacklightEj";
		} else if (realFramebuffer == &kextIntelAzulFb) { // Haswell
			maxBacklightFrequency = static_cast<FramebufferHSW *>(gPlatformInformationList)[0].fBacklightMax;
			hwSetBacklightSym = "__ZN24AppleIntelAzulController14hwSetBacklightEj";
		} else if (realFramebuffer == &kextIntelBDWFb) { // Broadwell
			maxBacklightFrequency = static_cast<FramebufferBDW *>(gPlatformInformationList)[0].fBacklightMax;
			hwSetBacklightSym = "";
		} else if (realFramebuffer == &kextIntelSKLFb) { // Skylake
			maxBacklightFrequency = static_cast<FramebufferSKL *>(gPlatformInformationList)[0].fBacklightMax;
			hwSetBacklightSym = "__ZN31AppleIntelFramebufferController14hwSetBacklightEj";
		} else if (realFramebuffer == &kextIntelKBLFb) { // Kaby Lake
			maxBacklightFrequency = static_cast<FramebufferSKL *>(gPlatformInformationList)[0].fBacklightMax;
			hwSetBacklightSym = "__ZN31AppleIntelFramebufferController14hwSetBacklightEj";
		} else { // Coffee Lake and newer
			//FIXME: Find a way to get hardcoded value
			maxBacklightFrequency = 7777; // or 22222
			hwSetBacklightSym = "__ZN31AppleIntelFramebufferController14hwSetBacklightEj";
		}

		DBGLOG("smoother", "maxBacklightFrequency = 0x%x", maxBacklightFrequency);
		DBGLOG("smoother", "hwSetBacklight symbol %s", hwSetBacklightSym);

		auto solvedSymbol = patcher.solveSymbol(index, hwSetBacklightSym, address, size);
		if (!solvedSymbol) {
			SYSLOG("smoother", "Failed to find hwSetBacklight");
			return;
		}

		orgHwSetBacklight = reinterpret_cast<decltype(orgHwSetBacklight)>(patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(solvedSymbol), reinterpret_cast<mach_vm_address_t>(wrapHwSetBacklight), true));
		if (!orgHwSetBacklight) {
			SYSLOG("smoother", "Failed to route hwSetBacklight");
			return;
		}

		DBGLOG("smoother", "Successfully routed hwSetBacklight");
	}
}
