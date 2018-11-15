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

		template <typename T>
		bool Read(int32_t msgId, int32_t version, T& t) {
			int32_t size = 0;
			const char * data = ReadFrame(size);
			if (data == nullptr)
				return false;

			if (*(int32_t*)data != msgId)
				return false;

			hn_istream stream(data + sizeof(int32_t), size - sizeof(int32_t));
			hn_iachiver ar(stream, 0);

			ar >> t;
			if (ar.Fail())
				return false;

			return true;
		}

		template <int32_t len, typename T>
		void Write(int32_t msgId, int32_t version, T & t) {
			uint8_t buffer[len + 32 + sizeof(msgId)];

			*(int32_t*)(buffer + 32) = msgId;

			hn_ostream stream((char *)buffer + 32 + sizeof(msgId), len);
			hn_oachiver ar(stream, version);
			ar << t;
			if (ar.Fail())
				return;

			int32_t size = (int32_t)stream.size();
			int32_t offset = 1;
			if (size <= 125)
				offset += 1;
			else if (size <= 65535) {
				offset += 3;
			}
			else if (size <= 65535) {
				offset += 9;
			}

			int32_t pos = 32 - offset;
			buffer[pos++] = 0x02;

			if (size <= 125) {
				buffer[pos++] = size;
			}
			else if (size <= 65535) {
				buffer[pos++] = 126;

				buffer[pos++] = (size >> 8) & 0xFF;
				buffer[pos++] = size & 0xFF;
			}
			else {
				buffer[pos++] = 127;

				for (int i = 3; i >= 0; i--) {
					buffer[pos++] = 0;
				}

				for (int i = 3; i >= 0; i--) {
					buffer[pos++] = ((size >> 8 * i) & 0xFF);
				}
			}

			hn_send(_fd, (const char *)buffer + 32 -  offset, size + offset);
		};

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

	class WebSocketSender {
	public:
		WebSocketSender(int32_t fd) : _fd(fd) {}
		~WebSocketSender() {}

		template <int32_t len, typename T>
		void Write(int32_t msgId, int32_t version, T & t) {
			uint8_t buffer[len + 32 + sizeof(msgId)];

			*(int32_t*)(buffer + 32) = msgId;

			hn_ostream stream((char *)buffer + 32 + sizeof(msgId), len);
			hn_oachiver ar(stream, version);
			ar << t;
			if (ar.Fail())
				return;

			int32_t size = (int32_t)stream.size();
			int32_t offset = 1;
			if (size <= 125)
				offset += 1;
			else if (size <= 65535) {
				offset += 3;
			}
			else if (size <= 65535) {
				offset += 9;
			}

			int32_t pos = 32 - offset;
			buffer[pos++] = 0x02;

			if (size <= 125) {
				buffer[pos++] = size;
			}
			else if (size <= 65535) {
				buffer[pos++] = 126;

				buffer[pos++] = (size >> 8) & 0xFF;
				buffer[pos++] = size & 0xFF;
			}
			else {
				buffer[pos++] = 127;

				for (int i = 3; i >= 0; i--) {
					buffer[pos++] = 0;
				}

				for (int i = 3; i >= 0; i--) {
					buffer[pos++] = ((size >> 8 * i) & 0xFF);
				}
			}

			hn_send(_fd, (const char *)buffer + 32 - offset, size + offset);
		};

	private:
		int32_t _fd;
	};
}

#endif // !__WEBSOCKET_H__
