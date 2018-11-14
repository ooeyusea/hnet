#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__
#include "hnet.h"
#include <string>
#include <vector>

namespace websocket {
	enum WebSocketFrameType {
		ERROR_FRAME = 0xFF00,
		INCOMPLETE_FRAME = 0xFE00,

		OPENING_FRAME = 0x3300,
		CLOSING_FRAME = 0x3400,

		INCOMPLETE_TEXT_FRAME = 0x01,
		INCOMPLETE_BINARY_FRAME = 0x02,

		TEXT_FRAME = 0x81,
		BINARY_FRAME = 0x82,

		PING_FRAME = 0x19,
		PONG_FRAME = 0x1A
	};

	struct Frame {
		const char * start;
		int32_t size;



		const char * payload;
		int32_t payloadSize;
	};

	class WebSocket {
	public:
		WebSocket(int32_t fd);
		~WebSocket();

		bool ShakeHands();
		const char * ReadFrame(int32_t & size);

		void SendFrame(const char * data, int32_t size);
		void Close();
		inline void Shutdown() {
			hn_shutdown(_fd);
			_fd = -1;
		}

		inline bool IsConnected() const {
			if (!_connected || _fd < 0)
				return false;

			return hn_test_fd(_fd);
		}

		inline int32_t GetFd() const { return _fd; }

	private:
		WebSocketFrameType ParseHandshake(uint8_t * input_frame, int32_t input_len);
		std::string AnswerHandshake();
		WebSocketFrameType ParseFrame(uint8_t* buff, int32_t len, Frame& frame);
		void AnwserPongFrame(const Frame & frame);

		std::string Trim(std::string && str);
		std::vector<std::string> Explode(const std::string& theString, const char * theDelimiter, bool theIncludeEmptyStrings = false);

	private:
		int32_t _fd;

		char * _singlePacket;
		int32_t _appendPos = 0;

		char * _buff;
		int32_t _recvPos = 0;
		int32_t _parsePos = 0;
		bool _waitParsed = false;

		std::string _resource;
		std::string _host;
		std::string _origin;
		std::string _protocol;
		std::string _key;

		bool _connected = false;
	};
}

#endif // !__WEBSOCKET_H__
