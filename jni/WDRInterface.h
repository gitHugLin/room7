#ifndef __WDR_INTERFACE__
#define __WDR_INTERFACE__

enum {
	WDR_INPUT_PGM,
	WDR_INPUT_RGB,
	WDR_INPUT_YUV,
	WDR_INPUT_STREAM,
	WDR_OUTPUT_RGB,
	WDR_OUTPUT_YUV
};

class WDRInterface {

public:
	WDRInterface();
	~WDRInterface();
public:
	bool initialized;
	int initialize(int width, int height);
	int process(int data, int mode);
	int deinit();
};
#endif
