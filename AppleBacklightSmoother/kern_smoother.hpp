//
//  kern_smoother.cpp
//  AppleBacklightSmoother
//
//  Copyright © 2020 Le Bao Hiep. All rights reserved.
//

#ifndef kern_smoother_hpp
#define kern_smoother_hpp

static const char *pathIntelHDFb[]    { "/System/Library/Extensions/AppleIntelHDGraphicsFB.kext/Contents/MacOS/AppleIntelHDGraphicsFB" };
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

static KernelPatcher::KextInfo kextIntelHDFb    { "com.apple.driver.AppleIntelHDGraphicsFB", pathIntelHDFb, arrsize(pathIntelHDFb), {}, {}, KernelPatcher::KextInfo::Unloaded };
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

static constexpr uint32_t BLC_PWM_CPU_CTL = 0x48254;
static constexpr uint32_t BXT_BLC_PWM_CTL1 = 0xC8250;
static constexpr uint32_t BXT_BLC_PWM_FREQ1 = 0xC8254;
static constexpr uint32_t BXT_BLC_PWM_DUTY1 = 0xC8258;

namespace AppleBacklightSmootherNS {
	static IOWorkLoop *workLoop;
	static IOCommandGate *cmdGate;

	static KernelPatcher::KextInfo *currentFramebuffer;
	static KernelPatcher::KextInfo *currentFramebufferOpt;

	static uint32_t (*orgReadRegister32)(void *, uint32_t);
	static void (*orgWriteRegister32)(void *, uint32_t, uint32_t);

	static constexpr uint32_t FallbackTargetBacklightFrequency {120000};

	static bool backlightValueAssigned;
	static uint32_t lastBacklightValue;
	static uint32_t targetBacklightFrequency;
	static uint32_t targetPwmControl;
	static uint32_t driverBacklightFrequency;

	static void init_plugin();

	static void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	static constexpr uint32_t START_VALUE = 10;
	static constexpr uint32_t STEPS = 128;
	static uint32_t dutyTables[STEPS];
	static void generateTables();

	static void wrapIvyWriteRegister32(void *that, uint32_t reg, uint32_t value);
	static void wrapHswWriteRegister32(void *that, uint32_t reg, uint32_t value);
	static void wrapCflRealWriteRegister32(void *that, uint32_t reg, uint32_t value);
	static void wrapCflFakeWriteRegister32(void *that, uint32_t reg, uint32_t value);
}

class EXPORT PRODUCT_NAME : public IOService {
	OSDeclareDefaultStructors(PRODUCT_NAME)
public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
};

extern PRODUCT_NAME *ADDPR(selfInstance);

#endif /* kern_smoother_hpp */
