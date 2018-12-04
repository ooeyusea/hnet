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
		hn_error("initialize step {} looped.", step.name);
		return false;
	}

	step.traveled = true;

	for (auto & must : step.must) {
		auto itr = _steps.find(must);
		if (itr != _steps.end()) {
			if (!DoStep(itr->second))
				return false;
		}
		else {
			hn_error("initialize step {} need init pre step {}.", step.name, must);
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

	if (!step.fn()) {
		hn_error("initialize step {} failed.", step.name);
		return false;
	}

	step.inited = true;
	hn_info("initialize step {} complete.", step.name);
	return true;
}
