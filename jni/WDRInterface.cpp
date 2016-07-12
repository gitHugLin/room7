#include "WDRBase.h"
#include "WDRInterface.h"
#include "cutils/properties.h"
#include "log.h"

using namespace wdr;

static wdrBase* mWDRBase;
static int mWidth, mHeight;
static char prop_val[PROPERTY_VALUE_MAX];
static bool wdr_enable = true;


WDRInterface::WDRInterface():initialized(false) {
	//LOGD("WDRInterface()");
}
WDRInterface::~WDRInterface() {
	deinit();
	LOGD("~WDRInterface()");
}

int WDRInterface::initialize(int width, int height) {
	if (initialized && width == mWidth && height == mHeight) {
		return 0;
	}
	
	LOGD("initialize()");
	mWidth = width;
	mHeight = height;

	if (mWDRBase != NULL) {
		deinit();
	}
	
	mWDRBase = new wdrBase();
	mWDRBase->initialize(width, height);
	initialized = true;
	property_get("sys.camera.wdr", prop_val, "1");
	if (!strcmp(prop_val, "1")) {
		wdr_enable = true;
	} else {
		wdr_enable = false;
	}
}

int WDRInterface::process(int data, int mode) {
	//OGD("process(data: %.8x, mode: %d)", data, mode);
	if (wdr_enable) {
		mWDRBase->process(data, mode);
	}
}

int WDRInterface::deinit() {
	mWDRBase = NULL;
	initialized = false;
}
