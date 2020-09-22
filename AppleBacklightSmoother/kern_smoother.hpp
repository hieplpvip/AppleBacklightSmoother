//
//  kern_smoother.cpp
//  AppleBacklightSmoother
//
//  Copyright © 2020 Le Bao Hiep. All rights reserved.
//

#ifndef kern_smoother_hpp
#define kern_smoother_hpp

#include "kern_fb.hpp"

#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>

class Smoother {
public:
	void init();

private:
	/**
	 *  Private self instance for callbacks
	 */
	static Smoother *callbackSmoother;

	/**
	 *  Current framebuffer kext used for modification
	 */
	KernelPatcher::KextInfo *currentFramebuffer {nullptr};

	/**
	 *  Current framebuffer optional kext used for modification
	 */
	KernelPatcher::KextInfo *currentFramebufferOpt {nullptr};

	/**
	 *  Framebuffer list, imported from the framebuffer kext
	 */
	void *gPlatformInformationList {nullptr};

	/**
	 *  Set to true if backlightValue has been assigned once
	 */
	bool backlightValueAssigned {false};

	/**
	 *  Current backlight duty cycle value
	 */
	uint32_t backlightValue {0};

	/**
	 *  Max backlight frequency obtained from framebuffer info
	 */
	uint32_t maxBacklightFrequency {};

	/**
	 *  Original AppleIntelFramebufferController::hwSetBacklight function
	 */
	uint32_t (*orgHwSetBacklight)(void *, uint32_t) {nullptr};

	/**
	 *  AppleIntelFramebufferController::hwSetBacklight wrapper to smooth backlight transition
	 */
	static IOReturn wrapHwSetBacklight(void *that, uint32_t backlight);

	/**
	 *  Patch kext if needed and prepare other patches
	 *
	 *  @param patcher KernelPatcher instance
	 *  @param index   kinfo handle
	 *  @param address kinfo load address
	 *  @param size    kinfo memory size
	 */
	void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
};

#endif /* kern_smoother_hpp */
