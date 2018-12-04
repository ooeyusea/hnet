#ifndef __MYSQLMGR_H__
#define __MYSQLMGR_H__
#include "hnet.h"
#ifdef WIN32
#include "mysql.h"
#else
#include  <mysql/mysql.h>
#endif
#include <vector>
#include "conf.h"

class MysqlExecutor {
public:
	class ResultSet {
	public:
		ResultSet();
		~ResultSet();

		bool Next();
		bool Empty() { return _rowCount == 0; }

		int64_t ToInt64(uint32_t index);
		uint64_t ToUInt64(uint32_t index);
		int32_t ToInt32(uint32_t index);
		uint32_t ToUInt32(uint32_t index);
		int16_t ToInt16(uint32_t index);
		uint16_t ToUInt16(uint32_t index);
		int8_t ToInt8(uint32_t index);
		uint8_t ToUInt8(uint32_t index);
		float ToFloat(uint32_t index);
		double ToDouble(uint32_t index);
		const char * ToString(uint32_t index);
		int32_t GetData(uint32_t index, char* buffer, int32_t maxSize);

		void SetResult(MYSQL& mysql);
		void CloseResult();

	private:
		MYSQL_RES* _result;
		MYSQL_FIELD* _fields;
		MYSQL_ROW _row;
		unsigned long* _curRowFieldLengths;
		uint32_t _fieldCount;
		uint32_t _rowCount;
	};

	class Connection {
	public:
		Connection();
		~Connection();

		bool Open(const char * dsn);

		int32_t Execute(const char* sql);
		int32_t Execute(const char* sql, ResultSet& rs);

		inline int32_t EscapeString(const char * src, int size, char * dst) {
			return (int32_t)mysql_real_escape_string(&_handler, dst, src, size);
		}

	private:
		bool Reopen();
		bool Ping();

	private:
		MYSQL _handler;

		std::string _user;
		std::string _passwd;
		std::string _dbName;
		std::string _host;
		std::string _unixHandler;
		int32_t _port = 3306;
		uint32_t _clientFlag;
		std::string  _charset;
	};

public:
	MysqlExecutor(hyper_net::IAsyncQueue * queue) : _queue(queue) {}
	~MysqlExecutor() {}

	bool Open(const char * dsn, int32_t threadCount);

	int32_t Execute(uint64_t idx, const char* sql);
	int32_t Execute(uint64_t idx, const char* sql, ResultSet& rs);

	inline int32_t EscapeString(const char * src, int size, char * dst) {
		return _connections.front()->EscapeString(src, size, dst);
	}

	inline int32_t OnExecute(uint64_t idx, const char* sql) {
		return _connections[idx % _connections.size()]->Execute(sql);
	}

	inline int32_t OnExecute(uint64_t idx, const char* sql, ResultSet& rs) {
		return _connections[idx % _connections.size()]->Execute(sql, rs);
	}

private:
	std::vector<Connection*> _connections;
	hyper_net::IAsyncQueue * _queue;
};

class MysqlMgr : public Conf {
public:
	static MysqlMgr& Instance() {
		static MysqlMgr g_instance;
		return g_instance;
	}

	bool Start();

	MysqlExecutor * Open(const char * dsn);
private:
	MysqlMgr() {}
	~MysqlMgr() {}

	hyper_net::IAsyncQueue * _queue = nullptr;
	int32_t _threadCount = 0;
};

#endif //__MYSQLMGR_H__
