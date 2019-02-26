#ifndef __HNET_STREAM_H__
#define __HNET_STREAM_H__
#include <string>
#include <ios>
#include <string.h>
#include <vector>
#include <tuple>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>

namespace hyper_net {
	struct DeliverBuffer {
		const char * buff;
		int32_t size;
	};

	template <typename AR, typename T>
	struct OArchiverHelper {
		static AR& invoke(AR& ar, T& t) {
			t.Archive(ar);
			return ar;
		}
	};

	template <typename AR, typename T>
	struct OArchiverHelper<AR, std::vector<T>> {
		static AR& invoke(AR& ar, std::vector<T>& container) {
			ar & (uint32_t)container.size();
			for (auto& t : container) {
				ar & t;
			}

			return ar;
		}
	};

	template <typename AR, typename T>
	struct OArchiverHelper<AR, std::list<T>> {
		static AR& invoke(AR& ar, std::list<T>& container) {
			ar & (uint32_t)container.size();
			for (auto& t : container) {
				ar & t;
			}

			return ar;
		}
	};

	template <typename AR, typename T>
	struct OArchiverHelper<AR, std::set<T>> {
		static AR& invoke(AR& ar, std::set<T>& container) {
			ar & (uint32_t)container.size();
			for (auto& t : container) {
				ar & t;
			}

			return ar;
		}
	};

	template <typename AR, typename T>
	struct OArchiverHelper<AR, std::unordered_set<T>> {
		static AR& invoke(AR& ar, std::unordered_set<T>& container) {
			ar & (uint32_t)container.size();
			for (auto& t : container) {
				ar & t;
			}

			return ar;
		}
	};

	template <typename AR, typename K, typename V>
	struct OArchiverHelper<AR, std::map<K, V>> {
		static AR& invoke(AR& ar, std::map<K, V>& container) {
			ar & (uint32_t)container.size();
			for (auto& t : container) {
				ar & t.first;
				ar & t.second;
			}

			return ar;
		}
	};

	template <typename AR, typename K, typename V>
	struct OArchiverHelper<AR, std::unordered_map<K, V>> {
		static AR& invoke(AR& ar, std::unordered_map<K, V>& container) {
			ar & (uint32_t)container.size();
			for (auto& t : container) {
				ar & t.first;
				ar & t.second;
			}

			return ar;
		}
	};

	template <typename AR, typename T, int32_t Size>
	struct OArchiverHelper<AR, T[Size]> {
		static AR& invoke(AR& ar, T(&arr)[Size]) {
			for (int32_t i = 0; i < Size; ++i) {
				ar & arr[i];
			}
			return ar;
		}
	};

	template <class Stream>
	class OArchiver {
	public:
		OArchiver(Stream& stream, int32_t version) : _stream(stream), _version(version) {}

		template <typename T>
		OArchiver& operator<<(T& t) {
			return *this & t;
		}

		template <typename T>
		OArchiver& operator&(T& t) {
			return OArchiverHelper<OArchiver, T>::invoke(*this, t);
		}

		template <int32_t Size>
		OArchiver& operator&(char(&array)[Size]) {
			_stream.write(array, Size);
			return *this;
		}

		OArchiver& operator&(std::string& str) {
			*this & (int16_t)str.size();
			_stream.write(str.c_str(), str.size());

			return *this;
		}

		OArchiver& operator&(const std::string& str) {
			*this & (int16_t)str.size();
			_stream.write(str.c_str(), str.size());
			return *this;
		}

		OArchiver& operator&(char* str) {
			int16_t len = strlen(str);
			*this & len;
			_stream.write(str, len);
			return *this;
		}

		OArchiver& operator&(const char* str) {
			int16_t len = strlen(str);
			*this & len;
			_stream.write(str, len);
			return *this;
		}

		OArchiver& operator&(DeliverBuffer& buf) {
			*this & buf.size;
			_stream.write(buf.buff, buf.size);
			return *this;
		}

		OArchiver& operator&(int8_t val) {
			_stream.write(&val, 1);
			return *this;
		}

		OArchiver& operator&(uint8_t val) {
			_stream.write((char*)&val, 1);
			return *this;
		}

		OArchiver& operator&(bool b) {
			char c = b ? 1 : 0;
			_stream.write(&c, 1);
			return *this;
		}

		OArchiver& operator&(int16_t val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(uint16_t val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(int32_t val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(uint32_t val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(int64_t val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(uint64_t val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(float val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		OArchiver& operator&(double val) {
			_stream.write((char*)&val, sizeof(val));
			return *this;
		}

		bool Fail() { return _stream.fail(); }
		int32_t Version() { return _version; }

	private:
		Stream& _stream;
		int32_t _version;
	};

	template <typename AR, typename T>
	struct IArchiverHelper {
		static AR& invoke(AR& ar, T& t) {
			t.Archive(ar);
			return ar;
		}
	};

	template <typename AR, typename T>
	struct IArchiverHelper<AR, std::vector<T>> {
		static AR& invoke(AR& ar, std::vector<T>& container) {
			uint32_t size = 0;
			ar & size;

			if (ar.Fail())
				return ar;

			for (uint32_t i = 0; i < size; ++i) {
				if (!ar.Fail()) {
					T t;
					ar & t;
					container.push_back(t);
				}
				else
					break;
			}

			return ar;
		}
	};

	template <typename AR, typename T>
	struct IArchiverHelper<AR, std::list<T>> {
		static AR& invoke(AR& ar, std::list<T>& container) {
			uint32_t size = 0;
			ar & size;

			if (ar.Fail())
				return ar;

			for (uint32_t i = 0; i < size; ++i) {
				if (!ar.Fail()) {
					T t;
					ar & t;
					container.push_back(t);
				}
				else
					break;
			}

			return ar;
		}
	};

	template <typename AR, typename T>
	struct IArchiverHelper<AR, std::set<T>> {
		static AR& invoke(AR& ar, std::set<T>& container) {
			uint32_t size = 0;
			ar & size;

			if (ar.Fail())
				return ar;

			for (uint32_t i = 0; i < size; ++i) {
				if (!ar.Fail()) {
					T t;
					ar & t;
					container.push_back(t);
				}
				else
					break;
			}

			return ar;
		}
	};

	template <typename AR, typename T>
	struct IArchiverHelper<AR, std::unordered_set<T>> {
		static AR& invoke(AR& ar, std::unordered_set<T>& container) {
			uint32_t size = 0;
			ar & size;

			if (ar.Fail())
				return ar;

			for (uint32_t i = 0; i < size; ++i) {
				if (!ar.Fail()) {
					T t;
					ar & t;
					container.push_back(t);
				}
				else
					break;
			}

			return ar;
		}
	};

	template <typename AR, typename K, typename V>
	struct IArchiverHelper<AR, std::map<K, V>> {
		static AR& invoke(AR& ar, std::map<K, V>& container) {
			uint32_t size = 0;
			ar & size;

			if (ar.Fail())
				return ar;

			for (uint32_t i = 0; i < size; ++i) {
				if (!ar.Fail()) {
					K k;
					V v;
					ar & k;
					ar & v;

					container.insert(std::make_pair(k, v));
				}
				else
					break;
			}

			return ar;
		}
	};

	template <typename AR, typename K, typename V>
	struct IArchiverHelper<AR, std::unordered_map<K, V>> {
		static AR& invoke(AR& ar, std::unordered_map<K, V>& container) {
			uint32_t size = 0;
			ar & size;

			if (ar.Fail())
				return ar;

			for (uint32_t i = 0; i < size; ++i) {
				if (!ar.Fail()) {
					K k;
					V v;
					ar & k;
					ar & v;

					container.insert(std::make_pair(k, v));
				}
				else
					break;
			}

			return ar;
		}
	};

	template <typename AR, typename T, int32_t Size>
	struct IArchiverHelper<AR, T[Size]> {
		static AR& invoke(AR& ar, T(&arr)[Size]) {
			for (int32_t i = 0; i < Size; ++i) {
				if (!ar.Fail())
					ar & arr[i];
				else
					break;
			}

			return ar;
		}
	};

	template <class Stream>
	class IArchiver {
	public:
		IArchiver(Stream& stream, int32_t version) : _stream(stream), _version(version) {}

		template <typename T>
		IArchiver& operator>>(T& t) {
			return *this & t;
		}

		template <typename T>
		IArchiver& operator&(T& t) {
			return IArchiverHelper<IArchiver, T>::invoke(*this, t);
		}

		template <int32_t Size>
		IArchiver& operator& (char(&array)[Size]) {
			if (!_stream.fail()) 
				_stream.read(array, Size);
			return *this;
		}

		IArchiver& operator&(std::string& str) {
			int16_t len = 0;
			*this & len;
			_stream.read(str, len);

			return *this;
		}

		IArchiver& operator&(DeliverBuffer& buf) {
			*this & buf.size;
			_stream.read(buf, buf.size);
			return *this;
		}

		IArchiver& operator&(int8_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, 1);
			return *this;
		}

		IArchiver& operator&(uint8_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, 1);
			return *this;
		}

		IArchiver& operator&(bool & b) {
			char c = 0;
			if (!_stream.fail()) _stream.read((char*)&c, 1);

			b = (c != 0);
			return *this;
		}

		IArchiver& operator&(int16_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(short));
			return *this;
		}

		IArchiver& operator&(uint16_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(uint16_t));
			return *this;
		}

		IArchiver& operator&(int32_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(int32_t));
			return *this;
		}

		IArchiver& operator&(uint32_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(uint32_t));
			return *this;
		}

		IArchiver& operator&(int64_t & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(int64_t));
			return *this;
		}

		IArchiver& operator&(uint64_t& val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(uint64_t));
			return *this;
		}

		IArchiver& operator&(float & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(float));
			return *this;
		}

		IArchiver& operator&(double & val) {
			if (!_stream.fail()) 
				_stream.read((char*)&val, sizeof(double));
			return *this;
		}

		bool Fail() { return _stream.fail(); }

		int32_t Version() { return _version; }

	private:
		Stream& _stream;
		int32_t _version;
	};

	class IBufferStream {
	public:
		IBufferStream(const char* buf, int len)
			: _content(buf)
			, _len(len)
			, _pos(0)
			, _rdstate(std::ios_base::goodbit) {

		}

		~IBufferStream() {}

		void read(char * buf, size_t len) {
			if (_len < _pos + len)
				_rdstate |= std::ios_base::badbit;
			else {
				memcpy(buf, _content + _pos, len);
				_pos += len;

				if (_pos == _len)
					_rdstate |= std::ios_base::eofbit;
			}
		}

		void read(std::string& buf, size_t len) {
			if (_len < _pos + len)
				_rdstate |= std::ios_base::badbit;
			else {
				buf.append(_content + _pos, len);
				_pos += len;

				if (_pos == _len)
					_rdstate |= std::ios_base::eofbit;
			}
		}

		void read(DeliverBuffer& buf, size_t len) {
			if (_len < _pos + len)
				_rdstate |= std::ios_base::badbit;
			else {
				buf.buff = _content + _pos;
				_pos += len;

				if (_pos == _len)
					_rdstate |= std::ios_base::eofbit;
			}
		}

		bool good() const { return _rdstate == 0; }
		bool fail() const { return (_rdstate & (std::ios_base::badbit | std::ios_base::failbit)) != 0; }
		bool bad() const { return (_rdstate & std::ios_base::badbit) != 0; }
		bool eof() const { return (_rdstate & std::ios_base::eofbit) != 0; }

	private:
		const char* _content;
		size_t _len;
		size_t _pos;
		std::ios_base::iostate _rdstate;
	};


	class OBufferStream {
	public:
		OBufferStream(char* buf, int len)
			: _content(buf)
			, _len(len)
			, _pos(0)
			, _rdstate(std::ios_base::goodbit) {
		}

		~OBufferStream() {}

		void write(const char * buf, size_t len) {
			if (_len < _pos + len)
				_rdstate |= std::ios_base::badbit;
			else {
				memcpy(_content + _pos, buf, len);
				_pos += len;
			}
		}

		bool good() const { return _rdstate == 0; }
		bool fail() const { return (_rdstate & (std::ios_base::badbit | std::ios_base::failbit)) != 0; }
		bool bad() const { return (_rdstate & std::ios_base::badbit) != 0; }
		bool eof() const { return (_rdstate & std::ios_base::eofbit) != 0; }

		size_t size() { return _pos; }

		std::string str() { return std::string(_content, _pos); }

	private:
		char* _content;
		size_t _len;
		size_t _pos;
		std::ios_base::iostate _rdstate;
	};
}

#define hn_deliver_buffer hyper_net::DeliverBuffer

#define hn_iachiver hyper_net::IArchiver<hyper_net::IBufferStream>
#define hn_oachiver hyper_net::OArchiver<hyper_net::OBufferStream>

#define hn_istream hyper_net::IBufferStream
#define hn_ostream hyper_net::OBufferStream

#endif // !__HNET_STREAM_H__
