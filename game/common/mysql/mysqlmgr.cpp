#include "mysqlmgr.h"
#include <stdlib.h>
#include <string.h>
#include <thread>
#include "XmlReader.h"
#include "argument.h"

#define RECONNECT_SLEEP_TIME 300
#define RECONNECT_TIME_OUT 10000

MysqlExecutor::Connection::Connection() {
	mysql_init(&_handler);
}

MysqlExecutor::Connection::~Connection() {
	mysql_close(&_handler);
}

bool MysqlExecutor::Connection::Open(const char * dsn) {
	const char * start = strstr(dsn, ":");
	if (!start) {
		hn_error("dsn format failed {} ", dsn);
		return false;
	}

	bool last = false;
	const char * blockStart = start + 1;
	while (!last) {
		const char * blockEnd = strstr(blockStart, ";");
		if (!blockEnd) {
			blockEnd = blockStart + strlen(blockStart);
			last = true;
		}

		const char * seq = strstr(blockStart, "=");
		if (!seq) {
			hn_error("dsn format failed {} ", dsn);
			return false;
		}

		std::string key(blockStart, seq);
		std::string value(seq + 1, blockEnd);

		if (key == "user")
			_user = value;
		else if (key == "password")
			_passwd = value;
		else if (key == "host")
			_host = value;
		else if (key == "dbname")
			_dbName = value;
		else if (key == "charset")
			_charset = value;
		else if (key == "unix_socket")
			_unixHandler = value;
		else if (key == "port")
			_port = atoi(value.c_str());
		else if (key == "client_flag")
			_clientFlag = (uint32_t)atoll(value.c_str());

		blockStart = blockEnd + 1;
	}
	return Reopen();
}

bool MysqlExecutor::Connection::Reopen() {
	mysql_close(&_handler);
	mysql_init(&_handler);

	if (mysql_real_connect(&_handler, _host.c_str(), _user.c_str(), _passwd.c_str(), _dbName.c_str(), _port, _unixHandler.c_str(), _clientFlag) == NULL) {
		hn_error("open mysql {}:{} {} failed : {}", _host, _port, _dbName, mysql_error(&_handler));
		return false;
	}

	int value = 1;
	mysql_options(&_handler, MYSQL_OPT_RECONNECT, (char *)&value);

	if (!_charset.empty()) {
		if (mysql_set_character_set(&_handler, _charset.c_str())) {
			hn_error("open mysql {}:{} {} set charset {} failed : {}", _host, _port, _dbName, _charset, mysql_error(&_handler));
			return false;
		}
	}

	hn_info("open mysql {}:{} {} success", _host, _port, _dbName);
	return true;
}

bool MysqlExecutor::Connection::Ping() {
	int64_t tick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	while (mysql_ping(&_handler) != 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_SLEEP_TIME));

		int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		if (now > tick + RECONNECT_TIME_OUT)
			return Reopen();
	}
	return true;
}

int32_t MysqlExecutor::Connection::Execute(const char* sql) {
	int32_t ret = mysql_real_query(&_handler, sql, (unsigned long)strlen(sql));
	if (ret == 0)
		return (int32_t)mysql_affected_rows(&_handler);

	if (Ping()) {
		ret = mysql_real_query(&_handler, sql, (unsigned long)strlen(sql));
		if (ret == 0)
			return (int32_t)mysql_affected_rows(&_handler);
	}

	hn_error("execute failed {} for {}", mysql_error(&_handler), sql);

	return -1;
}

int32_t MysqlExecutor::Connection::Execute(const char* sql, ResultSet& rs) {
	int32_t ret = mysql_real_query(&_handler, sql, (unsigned long)strlen(sql));
	if (ret == 0) {
		rs.SetResult(_handler);
		return (int32_t)mysql_affected_rows(&_handler);
	}

	if (Ping()) {
		ret = mysql_real_query(&_handler, sql, (unsigned long)strlen(sql));
		if (ret == 0) {
			rs.SetResult(_handler);
			return (int32_t)mysql_affected_rows(&_handler);
		}
	}

	hn_error("execute failed {} for {}", mysql_error(&_handler), sql);

	return -1;
}

MysqlExecutor::ResultSet::ResultSet()
	: _result(NULL)
	, _fields(NULL)
	, _row(NULL)
	, _curRowFieldLengths(NULL)
	, _fieldCount(0)
	, _rowCount(0) {
}

MysqlExecutor::ResultSet::~ResultSet() {
	CloseResult();
}

void MysqlExecutor::ResultSet::SetResult(MYSQL& mysql) {
	_result = mysql_store_result(&mysql);
	if (_result) {
		_fields = mysql_fetch_fields(_result);
		_fieldCount = mysql_num_fields(_result);
		_rowCount = (uint32_t)mysql_num_rows(_result);
	}
}

void MysqlExecutor::ResultSet::CloseResult() {
	if (_result) {
		mysql_free_result(_result);
		_result = NULL;
	}
	_fields = NULL;
	_row = NULL;
	_curRowFieldLengths = NULL;
	_fieldCount = 0;
	_rowCount = 0;
}

bool MysqlExecutor::ResultSet::Next() {
	if (_result) {
		_row = mysql_fetch_row(_result);
		if (_row) {
			_curRowFieldLengths = mysql_fetch_lengths(_result);
			return true;
		}
	}
	return false;
}

int64_t MysqlExecutor::ResultSet::ToInt64(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return atoll(_row[index]);
	}
	return 0;
}

uint64_t MysqlExecutor::ResultSet::ToUInt64(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return strtoull(_row[index], NULL, 10);
	}
	return 0;
}

int32_t MysqlExecutor::ResultSet::ToInt32(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return atoi(_row[index]);
	}
	return 0;
}

uint32_t MysqlExecutor::ResultSet::ToUInt32(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return (uint32_t)atoll(_row[index]);
	}
	return 0;
}

int16_t MysqlExecutor::ResultSet::ToInt16(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return (int16_t)atoi(_row[index]);
	}
	return 0;
}

uint16_t MysqlExecutor::ResultSet::ToUInt16(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return (uint16_t)atoi(_row[index]);
	}
	return 0;
}

int8_t MysqlExecutor::ResultSet::ToInt8(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return (int8_t)atoi(_row[index]);
	}
	return 0;
}

uint8_t MysqlExecutor::ResultSet::ToUInt8(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;
		return (uint8_t)atoi(_row[index]);
	}
	return 0;
}

float MysqlExecutor::ResultSet::ToFloat(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0.0f;
		return (float)atof(_row[index]);
	}
	return 0.0f;
}

double MysqlExecutor::ResultSet::ToDouble(uint32_t index) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0.0;
		return atof(_row[index]);
	}
	return 0.0;
}

const char * MysqlExecutor::ResultSet::ToString(uint32_t index) {
	if (index < _fieldCount && _row[index])
		return _row[index];
	return "";
}

int32_t MysqlExecutor::ResultSet::GetData(uint32_t index, char* buffer, int32_t maxSize) {
	if (index < _fieldCount) {
		if (!_row[index]) 
			return 0;

		int len = 0;
		if (_curRowFieldLengths)
			len = _curRowFieldLengths[index];

		if (len > maxSize)
			len = maxSize;

		memcpy(buffer, _row[index], len);
		return len;
	}
	return -1;
}

struct MysqlExecuteInfo {
	MysqlExecutor * executor = nullptr;
	uint64_t idx = 0;
	const char * sql = nullptr;
	void * rs = nullptr;
	int32_t ret = -1;
};


bool MysqlExecutor::Open(const char * dsn, int32_t threadCount) {
	if (threadCount <= 0)
		return false;

	for (int32_t i = 0; i < threadCount; ++i) {
		Connection * conn = new Connection;
		if (!conn->Open(dsn)) {
			delete conn;
			return false;
		}

		_connections.push_back(conn);
	}

	return true;
}

int32_t MysqlExecutor::Execute(uint64_t idx, const char* sql) {
	MysqlExecuteInfo info{ this, idx, sql };
	_queue->Call((uint32_t)idx, &info);
	return info.ret;
}

int32_t MysqlExecutor::Execute(uint64_t idx, const char* sql, ResultSet& rs) {
	MysqlExecuteInfo info{ this, idx, sql, &rs };
	_queue->Call((uint32_t)idx, &info);
	return info.ret;
}

bool MysqlMgr::Start() {
	try {
		olib::XmlReader conf;
		if (!conf.LoadXml(_conf.c_str())) {
			return false;
		}

		_threadCount = conf.Root()["mysql"][0].GetAttributeInt32("thread_count");

		hn_info("mysql thread count {}", _threadCount);
	}
	catch (std::exception& e) {
		hn_error("read mysql config failed {}", e.what());
		return false;
	}

	_queue = hn_create_async(_threadCount, true, [this](void * src) {
		MysqlExecuteInfo& info = *(MysqlExecuteInfo*)src;
		if (info.rs)
			info.ret = info.executor->OnExecute(info.idx, info.sql, *(MysqlExecutor::ResultSet*)info.rs);
		else
			info.ret = info.executor->OnExecute(info.idx, info.sql);
	});

	hn_info("mysql start complete");
	return true;
}

MysqlExecutor * MysqlMgr::Open(const char * dsn) {
	MysqlExecutor * executor = new MysqlExecutor(_queue);
	if (!executor->Open(dsn, _threadCount)) {
		delete executor;
		hn_error("mysql open failed dsn {} ", dsn);
		return nullptr;
	}

	hn_info("mysql open dsn {} ", dsn);
	return executor;
}
