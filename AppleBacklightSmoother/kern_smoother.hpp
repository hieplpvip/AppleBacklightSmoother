//
//  kern_smoother.cpp
//  AppleBacklightSmoother
//
//  Copyright © 2020 Le Bao Hiep. All rights reserved.
//

#ifndef kern_smoother_hpp
#define kern_smoother_hpp

class AppleBacklightSmootherController : public IOService {
	OSDeclareDefaultStructors(AppleBacklightSmootherController)

public:
	bool start(IOService *provider) override;

	/**
	 *  AppleIntelFramebufferController::hwSetBacklight gated wrapper to smooth backlight transition
	 */
	IOReturn wrapHwSetBacklightGated(void *that, uint32_t *backlight);
};

#endif /* kern_smoother_hpp */
