#ifndef __XMLREADER_H_
#define __XMLREADER_H_
#include "hnet.h"
#include <set>
#include <string>

namespace olib {
	class IXmlObject {
	public:
		virtual ~IXmlObject() {}

		virtual int8_t GetAttributeInt8(const char * attr) const = 0;
		virtual int16_t GetAttributeInt16(const char * attr) const = 0;
		virtual int32_t GetAttributeInt32(const char * attr) const = 0;
		virtual int64_t GetAttributeInt64(const char * attr) const = 0;
		virtual float GetAttributeFloat(const char * attr) const = 0;
		virtual bool GetAttributeBoolean(const char * attr) const = 0;
		virtual const char * GetAttributeString(const char * attr) const = 0;
		virtual bool HasAttribute(const char * attr) const = 0;

		virtual const char * CData() const = 0;
		virtual const char * Text() const = 0;

		virtual const IXmlObject& operator[](const int32_t index) const = 0;
		virtual const int32_t Count() const = 0;

		virtual const IXmlObject& operator[](const char * name) const = 0;
		virtual bool IsExist(const char * name) const = 0;
	};

	class ArrayIterator {
	public:
		class iterator {
		public:
			iterator(const IXmlObject& object) : _object(object), _pos(0) {}
			iterator(const IXmlObject& object, int32_t end) : _object(object), _pos(end) {}

			inline iterator& operator++() {
				++_pos;
				return *this;
			}

			inline const IXmlObject& operator*() const {
				return _object[_pos];
			}

			inline bool operator==(const iterator& rhs) const {
				return _pos == rhs._pos;
			}

			inline bool operator!=(const iterator& rhs) const {
				return _pos != rhs._pos;
			}

		private:
			const IXmlObject& _object;
			int32_t _pos;

		};

		ArrayIterator(const IXmlObject& object) : _object(object), _last(object, object.Count()) {}

		iterator begin() { return iterator(_object); }
		iterator& end() { return _last; }

	private:
		const IXmlObject& _object;
		iterator _last;
	};

    class XmlReader {
    public:
		XmlReader() : _root(nullptr) {}
		~XmlReader();

		bool LoadXml(const char * path);
		bool LoadXml(const void * data, const int32_t size);

		const IXmlObject& Root() const;

	private:
		bool LoadInclude(const char * path);
		bool LoadIncludeFile(const char * path);

    private:
		IXmlObject * _root;

		std::set<std::string> _loaded;
    };
}

#endif //__XMLREADER_H_
