#include "mysqlmgr.h"
#include <stdlib.h>
#include <string.h>
#include <thread>
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
		return false;
	}

	bool last = false;
	const char * blockStart = start;
	while (!last) {
		const char * blockEnd = strstr(blockStart, ";");
		if (!blockEnd) {
			blockEnd = blockStart + strlen(blockStart);
			last = true;
		}

		const char * seq = strstr(blockStart, "=");
		if (!seq) {
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
	}
	return Reopen();
}

bool MysqlExecutor::Connection::Reopen() {
	mysql_close(&_handler);
	mysql_init(&_handler);

	if (mysql_real_connect(&_handler, _host.c_str(), _user.c_str(), _passwd.c_str(), _dbName.c_str(), _port, _unixHandler.c_str(), _clientFlag) == NULL) {
		return false;
	}

	int value = 1;
	mysql_options(&_handler, MYSQL_OPT_RECONNECT, (char *)&value);

	if (!_charset.empty()) {
		if (mysql_set_character_set(&_handler, _charset.c_str())) {
			return false;
		}
	}

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
	int32_t ret = mysql_real_query(&_handler, sql, strlen(sql));
	if (ret == 0)
		return mysql_affected_rows(&_handler);

	if (Ping()) {
		ret = mysql_real_query(&_handler, sql, strlen(sql));
		if (ret == 0)
			return mysql_affected_rows(&_handler);
	}

	//mysql_errno(&_handler); mysql_error(&_handler);

	return -1;
}

int32_t MysqlExecutor::Connection::Execute(const char* sql, ResultSet& rs) {
	int32_t ret = mysql_real_query(&_handler, sql, strlen(sql));
	if (ret == 0) {
		rs.SetResult(_handler);
		return mysql_affected_rows(&_handler);
	}

	if (Ping()) {
		ret = mysql_real_query(&_handler, sql, strlen(sql));
		if (ret == 0) {
			rs.SetResult(_handler);
			return mysql_affected_rows(&_handler);
		}
	}

	//mysql_errno(&_handler); mysql_error(&_handler);

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
		_rowCount = mysql_num_rows(_result);
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
	uint64_t idx = 0;
	const char * sql = nullptr;
	void * rs = nullptr;
	int32_t ret = -1;
};

MysqlExecutor::MysqlExecutor() {

}

MysqlExecutor::~MysqlExecutor() {

}

bool MysqlExecutor::Open(const char * dsn, bool complete, int32_t threadCount) {
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

	_queue = hn_create_async(threadCount, complete, [this](void * src) {
		MysqlExecuteInfo& info = *(MysqlExecuteInfo*)src;
		if (info.rs)
			info.ret = OnExecute(info.idx, info.sql, *(ResultSet*)info.rs);
		else
			info.ret = OnExecute(info.idx, info.sql);
	});

	return true;
}

int32_t MysqlExecutor::Execute(uint64_t idx, const char* sql) {
	MysqlExecuteInfo info{ idx, sql };
	_queue->Call((uint32_t)idx, &info);
	return info.ret;
}

int32_t MysqlExecutor::Execute(uint64_t idx, const char* sql, ResultSet& rs) {
	MysqlExecuteInfo info{ idx, sql, &rs };
	_queue->Call((uint32_t)idx, &info);
	return info.ret;
}

MysqlExecutor * MysqlMgr::Start(const char * dsn, bool complete, int32_t threadCount) {
	MysqlExecutor * executor = new MysqlExecutor;
	if (!executor->Open(dsn, complete, threadCount)) {
		delete executor;
		return nullptr;
	}
	return executor;
}
