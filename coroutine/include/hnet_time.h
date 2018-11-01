#ifndef __HNET_TIME_H__
#define __HNET_TIME_H__

namespace hyper_net {
	struct TimeAdapter {
		int32_t operator-(int64_t millisecond);
		int32_t operator+(int64_t timestamp);
		int32_t operator+(const char * date);
	};

	class CountTickerImpl;
	class CountTicker {
	public:
		class iterator {
		public:
			iterator(CountTicker * ticker) : _ticker(ticker), _end(true) {}
			iterator(CountTicker * ticker, bool end) : _ticker(ticker), _end(end) { }
			~iterator() {}

			inline iterator& operator++() {
				_end = _ticker->Check();
				return *this;
			}

			inline int32_t operator*() const {
				return _ticker->Beat();
			}

			inline bool operator==(const iterator& rhs) const {
				return _end == rhs._end;
			}

			inline bool operator!=(const iterator& rhs) const {
				return _end != rhs._end;
			}

		private:
			bool _end = false;
			CountTicker * _ticker = nullptr;
		};

	public:
		CountTicker(int64_t interval, int32_t count);
		CountTicker(int64_t interval);
		~CountTicker();

		iterator begin() { iterator ret(this, false); return ++ret; }
		iterator& end() { return _last; }

		int32_t Beat();
		bool Check();
		void Stop();
		int32_t GetLastError() const;

	private:
		iterator _last;
		CountTickerImpl * _impl;
	};
}

#define hn_sleep hyper_net::TimeAdapter()-
#define hn_wait hyper_net::TimeAdapter()+
#define hn_ticker hyper_net::CountTicker

#endif // !__HNET_TIME_H__
