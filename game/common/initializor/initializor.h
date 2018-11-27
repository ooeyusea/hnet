#ifndef __INITIALIZOR_H__
#define __INITIALIZOR_H__
#include "util.h"
#include <map>
#include <vector>

class InitializorMgr {
	struct Step {
		std::string name;
		std::function<bool()> fn;
		std::vector<std::string> must;
		std::vector<std::string> maybe;

		bool inited = false;
		bool traveled = false;
		std::vector<Step *> nexts;
	};

public:
	class StepAdder {
	public:
		StepAdder(Step& step) : _step(step) {}

		inline StepAdder& MustInitAfter(const char * name) {
			_step.must.push_back(name);
			return *this;
		}

		inline StepAdder& InitAfterIfHas(const char * name) {
			_step.maybe.push_back(name);
			return *this;
		}

	private:
		Step& _step;
	};

public:
	static InitializorMgr& Instance() {
		static InitializorMgr g_instance;
		return g_instance;
	}

	bool Start();

	inline StepAdder AddStep(const char * name, const std::function<bool()> & f) {
		_steps[name] = { name, f };
		return StepAdder(_steps[name]);
	}

private:
	bool DoStep(Step& step);

private:
	InitializorMgr() {}
	~InitializorMgr() {}

	std::map<std::string, Step> _steps;
};

#endif // !__INITIALIZOR_H__
