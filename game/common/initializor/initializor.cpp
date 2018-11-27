#include "initializor.h"

bool InitializorMgr::Start() {
	for (auto itr = _steps.begin(); itr != _steps.end(); ++itr) {
		if (!DoStep(itr->second))
			return false;
	}
	return true;
}

bool InitializorMgr::DoStep(Step& step) {
	if (step.inited)
		return true;

	if (step.traveled) {
		return false;
	}

	step.traveled = true;

	if (!step.fn())
		return false;

	for (auto & must : step.must) {
		auto itr = _steps.find(must);
		if (itr != _steps.end()) {
			if (!DoStep(itr->second))
				return false;
		}
		else {
			return false;
		}
	}

	for (auto & maybe : step.maybe) {
		auto itr = _steps.find(maybe);
		if (itr != _steps.end()) {
			if (!DoStep(itr->second))
				return false;
		}
	}

	step.inited = true;
	return true;
}
