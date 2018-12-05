#include "XmlReader.h"
#include <vector>
#include <unordered_map>
#include "tinyxml.h"
#include <exception>
#include "file_system.h"

namespace olib {
	class XmlNull : public IXmlObject {
	public:
		XmlNull() {}
		virtual ~XmlNull() {}

		virtual int8_t GetAttributeInt8(const char * attr) const { throw std::logic_error("this is a null xml"); return 0; }
		virtual int16_t GetAttributeInt16(const char * attr) const { throw std::logic_error("this is a null xml"); return 0; }
		virtual int32_t GetAttributeInt32(const char * attr) const { throw std::logic_error("this is a null xml"); return 0; }
		virtual int64_t GetAttributeInt64(const char * attr) const { throw std::logic_error("this is a null xml"); return 0; }
		virtual float GetAttributeFloat(const char * attr) const { throw std::logic_error("this is a null xml"); return 0.f; }
		virtual bool GetAttributeBoolean(const char * attr) const { throw std::logic_error("this is a null xml"); return false; }
		virtual const char * GetAttributeString(const char * attr) const { throw std::logic_error("this is a null xml"); return nullptr; }
		virtual bool HasAttribute(const char * attr) const { throw std::logic_error("this is a null xml"); return false; }

		virtual const char * CData() const { throw std::logic_error("this is a null xml"); return nullptr; }
		virtual const char * Text() const { throw std::logic_error("this is a null xml"); return nullptr; }

		virtual const IXmlObject& operator[](const int32_t index) const { throw std::logic_error("this is a null xml"); return *this; }
		virtual const int32_t Count() const { throw std::logic_error("this is a null xml"); return 0; }

		virtual const IXmlObject& operator[](const char * name) const { throw std::logic_error("this is a null xml"); return *this; }
		virtual bool IsExist(const char * name) const { throw std::logic_error("this is a null xml"); return false; }
	};

	class XmlObject;
	class XmlArray : public IXmlObject {
	public:
		XmlArray() {}
		virtual ~XmlArray();

		void AddElement(const TiXmlElement * element);

		virtual int8_t GetAttributeInt8(const char * attr) const { throw std::logic_error("this is a array xml"); return 0; }
		virtual int16_t GetAttributeInt16(const char * attr) const { throw std::logic_error("this is a array xml"); return 0; }
		virtual int32_t GetAttributeInt32(const char * attr) const { throw std::logic_error("this is a array xml"); return 0; }
		virtual int64_t GetAttributeInt64(const char * attr) const { throw std::logic_error("this is a array xml"); return 0; }
		virtual float GetAttributeFloat(const char * attr) const { throw std::logic_error("this is a array xml"); return 0.f; }
		virtual bool GetAttributeBoolean(const char * attr) const { throw std::logic_error("this is a array xml"); return false; }
		virtual const char * GetAttributeString(const char * attr) const { throw std::logic_error("this is a array xml"); return nullptr; }
		virtual bool HasAttribute(const char * attr) const { throw std::logic_error("this is a array xml"); return false; }

		virtual const char * CData() const { throw std::logic_error("this is a array xml"); return nullptr; }
		virtual const char * Text() const { throw std::logic_error("this is a array xml"); return nullptr; }

		virtual const IXmlObject& operator[](const int32_t index) const;
		virtual const int32_t Count() const { return (int32_t)_elements.size(); }

		virtual const IXmlObject& operator[](const char * name) const { throw std::logic_error("this is a array xml"); return _null; }
		virtual bool IsExist(const char * name) const { throw std::logic_error("this is a array xml"); return false; }

	private:
		std::vector<XmlObject *> _elements;
		XmlNull _null;
	};

	class XmlObject : public IXmlObject{
		struct Value {
			std::string valueString;
			int64_t valueInt64;
			float valueFloat;
			bool valueBoolean;
		};

	public:
		XmlObject(const TiXmlElement * element) {
			LoadAttrs(element);
			LoadChildren(element);
			LoadText(element);
		}

		virtual ~XmlObject() {
			for (auto itr = _objects.begin(); itr != _objects.end(); ++itr)
				delete itr->second;
			_objects.clear();
		}

		virtual int8_t GetAttributeInt8(const char * attr) const { const Value * value = FindAttr(attr); return value ? (int8_t)value->valueInt64 : 0; }
		virtual int16_t GetAttributeInt16(const char * attr) const { const Value * value = FindAttr(attr); return value ? (int16_t)value->valueInt64 : 0; }
		virtual int32_t GetAttributeInt32(const char * attr) const { const Value * value = FindAttr(attr); return value ? (int32_t)value->valueInt64 : 0; }
		virtual int64_t GetAttributeInt64(const char * attr) const { const Value * value = FindAttr(attr); return value ? value->valueInt64 : 0; }
		virtual float GetAttributeFloat(const char * attr) const { const Value * value = FindAttr(attr); return value ? value->valueFloat : 0; }
		virtual bool GetAttributeBoolean(const char * attr) const { const Value * value = FindAttr(attr); return value ? value->valueBoolean : 0; }
		virtual const char * GetAttributeString(const char * attr) const { const Value * value = FindAttr(attr); return value ? value->valueString.c_str() : nullptr; }
		virtual bool HasAttribute(const char * attr) const { return FindAttr(attr) != nullptr; }

		virtual const char * CData() const {  return _text.c_str(); }
		virtual const char * Text() const { return _text.c_str();}

		virtual const IXmlObject& operator[](const int32_t index) const { throw std::logic_error("this is a obejct xml"); return _null; }
		virtual const int32_t Count() const { throw std::logic_error("this is a obejct xml"); return 0; }

		virtual const IXmlObject& operator[](const char * name) const {
			auto itr = _objects.find(name);
			if (itr == _objects.end()) {
				throw std::logic_error("where is child ???");
				return _null;
			}
			return *itr->second;
		}

		virtual bool IsExist(const char * name) const { return _objects.find(name) != _objects.end(); }

		void Append(const TiXmlElement * element) {
			LoadAttrs(element);
			LoadChildren(element);
			LoadText(element);
		}

	private:
		const Value * FindAttr(const char * attr) const {
			auto itr = _attrs.find(attr);
			if (itr != _attrs.end())
				return &itr->second;
			return nullptr;
		}

		void LoadAttrs(const TiXmlElement * element) {
			for (auto * attr = element->FirstAttribute(); attr; attr = attr->Next()) {
				const char * name = attr->Name();
				const char * value = attr->Value();

				Value unit;
				unit.valueString = value;
				unit.valueFloat = (float)atof(value);
				unit.valueInt64 = strtoull(value, nullptr, 10);
				unit.valueBoolean = (strcmp(value, "true") == 0);
				_attrs[attr->Name()] = unit;
			}
		}

		void LoadChildren(const TiXmlElement * element) {
			for (auto * node = element->FirstChildElement(); node; node = node->NextSiblingElement()) {
				if (_objects.find(node->Value()) == _objects.end())
					_objects[node->Value()] = new XmlArray;

				_objects[node->Value()]->AddElement(node);
			}
		}

		void LoadText(const TiXmlElement * element) {
			if (element->GetText())
				_text = element->GetText();
		}

	private:
		std::unordered_map<std::string, Value> _attrs;
		std::string _text;

		std::unordered_map<std::string, XmlArray*> _objects;
		XmlNull _null;
	};

	XmlArray::~XmlArray() {
		for (auto * element : _elements)
			delete element;
		_elements.clear();
	}

	void XmlArray::AddElement(const TiXmlElement * element) {
		_elements.push_back(new XmlObject(element));
	}

	const IXmlObject& XmlArray::operator[](const int32_t index) const {
		return *_elements[index];
	}

	XmlReader::~XmlReader() {
		if (_root)
			delete _root;
	}

	bool XmlReader::LoadXml(const char * path) {
		TiXmlDocument doc;
		if (!doc.LoadFile(path)) {
			throw std::logic_error(std::string("can't find xml file:") + path);
			return false;
		}

		const TiXmlElement * root = doc.RootElement();
		if (root == nullptr) {
			throw std::logic_error(std::string("core xml format error") + path);
			return false;
		}

		_loaded.insert(path);
		
		_root = new XmlObject(root);

#ifdef WIN32
		const char * baseEnd = strrchr(path, '\\');
#else
		const char * baseEnd = strrchr(path, '/');
#endif
		for (auto * node = root->FirstChildElement("include"); node; node = node->NextSiblingElement("include")) {
			std::string includePath = baseEnd ? (std::string(path, baseEnd - path) + "/" + node->Attribute("path")) : node->Attribute("path");
#ifdef WIN32
			std::transform(includePath.begin(), includePath.end(), includePath.begin(), [](char c) {
				if (c == '/')
					return '\\';
				return c;
			});
#endif
			if (!LoadInclude(includePath.c_str()))
				return false;
		}

		return true;
	}

	bool XmlReader::LoadXml(const void * data, const int32_t size) {
		TiXmlDocument doc;
		if (!doc.Parse((const char*)data)) {
			return false;
		}

		const TiXmlElement * root = doc.RootElement();
		if (root == nullptr) {
			throw std::logic_error("xml format error");
			return false;
		}

		_root = new XmlObject(root);
		return true;
	}

	const IXmlObject& XmlReader::Root() const {
		return *_root;
	}

	bool XmlReader::LoadInclude(const char * path) {
		olib::FileFinder().Search(path, [this](const std::string file){
			if (!LoadIncludeFile(file.c_str())) {
				throw std::logic_error(std::string("load include file") + file + " failed");
			}
		});

		return true;
	}

	bool XmlReader::LoadIncludeFile(const char * path) {
		if (_loaded.find(path) != _loaded.end())
			return true;

		TiXmlDocument doc;
		if (!doc.LoadFile(path)) {
			throw std::logic_error(std::string("can't find xml file:") + path);
			return false;
		}

		const TiXmlElement * root = doc.RootElement();
		if (root == nullptr) {
			throw std::logic_error(std::string("core xml format error") + path);
			return false;
		}

		_loaded.insert(path);

		((XmlObject*)_root)->Append(root);

#ifdef WIN32
		const char * baseEnd = strrchr(path, '\\');
#else
		const char * baseEnd = strrchr(path, '/');
#endif
		for (auto * node = root->FirstChildElement("include"); node; node = node->NextSiblingElement("include")) {
			std::string includePath = baseEnd ? (std::string(path, baseEnd - path) + "/" + node->Attribute("path")) : node->Attribute("path");
#ifdef WIN32
			std::transform(includePath.begin(), includePath.end(), includePath.begin(), [](char c) {
				if (c == '/')
					return '\\';
				return c;
			});
#endif
			if (!LoadInclude(includePath.c_str()))
				return false;
		}
		return true;
	}
}

