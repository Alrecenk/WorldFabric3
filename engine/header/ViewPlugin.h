#ifndef _VIEW_PLUGIN_H_
#define _VIEW_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "WorldPlugin.h"

class ViewPlugin : public AsyncPlugin {

public:
	
	ViewPlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

};
#endif // #ifndef _VIEW_PLUGIN_H_