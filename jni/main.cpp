#include "WDRBase.h"
#include "WDRInterface.h"

using namespace wdr;

int main() {
	LOGD("wdr initializing...\n");
	wdrBase *mWDRBase;
	mWDRBase = new wdrBase();
	mWDRBase->process(0, WDR_INPUT_YUV);
	mWDRBase = NULL;
	return 0;
}
