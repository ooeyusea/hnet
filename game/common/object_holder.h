#ifndef __OBJECT_HOLDER_H__
#define __OBJECT_HOLDER_H__

template <typename T, int32_t id = 0>
class Holder {
public:
	Holder(T * t) {
		g_instance = t;
	}

	~Holder() {

	}

	inline static T * Instance() { return g_instance; }

private:
	static T * g_instance;
};

template <typename T, int32_t id>
T * Holder<T, id>::g_instance = nullptr;

#endif //__OBJECT_HOLDER_H__