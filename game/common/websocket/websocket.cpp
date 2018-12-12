#include "websocket.h"
#include "sha1.h"
#include "base64.h"

#define SINGLE_PACKET_SIZE 4096
#define PACKET_BUFF_SIZE (SINGLE_PACKET_SIZE * 4)

namespace websocket {
	WebSocket::WebSocket(int32_t fd) : _fd(fd) {
		_singlePacket = new char[SINGLE_PACKET_SIZE];
		_buff = new char[PACKET_BUFF_SIZE];
	}

	WebSocket::~WebSocket() {
		delete _singlePacket;
		delete _buff;
	}

	bool WebSocket::ShakeHands() {
		char msg[SINGLE_PACKET_SIZE];
		int32_t pos = 0;
		int32_t len = hn_recv(_fd, msg + pos, sizeof(msg) - pos);
		while (len > 0) {
			pos += len;

			if (OPENING_FRAME == ParseHandshake((uint8_t*)msg, pos)) {

				std::string anwser = AnswerHandshake();
				hn_send(_fd, anwser.data(), (int32_t)anwser.size());
				return true;
			}

			len = hn_recv(_fd, msg + pos, sizeof(msg) - pos);
		}

		return false;
	}

	const char * WebSocket::ReadFrame(int32_t & size) {
		while (true) {
			if (_waitParsed) {
				Frame frame;
				auto ret = ParseFrame((uint8_t*)_buff + _parsePos, _recvPos - _parsePos, frame);
				if (ret != INCOMPLETE_FRAME && ret != ERROR_FRAME)
					_parsePos += frame.size;

				if (ret != INCOMPLETE_FRAME && ret != INCOMPLETE_TEXT_FRAME && ret != INCOMPLETE_BINARY_FRAME) {
					switch (ret) {
					case ERROR_FRAME: Shutdown(); return nullptr;
					case CLOSING_FRAME: Close(); return nullptr;
					case PING_FRAME: AnwserPongFrame(frame);  break;
					case PONG_FRAME: break;
					case TEXT_FRAME:
					case BINARY_FRAME: {
							if (_appendPos > 0) {
								memcpy(_singlePacket + _appendPos, frame.payload, frame.payloadSize);
								_appendPos += frame.payloadSize;
								size = _appendPos;
								_appendPos = 0;
								return _singlePacket;
							}
							else {
								size = frame.payloadSize;
								return frame.payload;
							}
						}
						break;
					}
				}
				else {
					if (ret == INCOMPLETE_TEXT_FRAME || ret == INCOMPLETE_BINARY_FRAME) {
						if (_appendPos + frame.payloadSize > SINGLE_PACKET_SIZE) {
							Shutdown(); 
							return nullptr;
						}

						memcpy(_singlePacket + _appendPos, frame.payload, frame.payloadSize);
						_appendPos += frame.payloadSize;
					}
					else {
						_waitParsed = false;

						if (_recvPos > _parsePos) {
							memmove(_buff, _buff + _parsePos, _recvPos - _parsePos);
							_recvPos -= _parsePos;
						}
						else
							_recvPos = 0;
					}
				}
			}

			if (!_waitParsed) {
				int32_t len = hn_recv(_fd, _buff + _recvPos, PACKET_BUFF_SIZE - _recvPos);
				if (len <= 0) {
					Shutdown();
					return nullptr;
				}

				_recvPos += len;
				_waitParsed = true;
			}
		}

		return nullptr;
	}

	void WebSocket::Close() {
		if (_connected) {
			if (_fd > 0) {

			}
		}

		hn_close(_fd);
		_fd = -1;
	}

	WebSocketFrameType WebSocket::ParseHandshake(uint8_t * input_frame, int32_t input_len) {
		std::string headers((char*)input_frame, input_len);
		std::string::size_type header_end = headers.find("\r\n\r\n");

		if (header_end == std::string::npos) { // end-of-headers not found - do not parse
			return INCOMPLETE_FRAME;
		}

		headers.resize(header_end); // trim off any data we don't need after the headers
		std::vector<std::string> headers_rows = Explode(headers, "\r\n");
		for (int32_t i = 0; i < headers_rows.size(); i++) {
			std::string& header = headers_rows[i];
			if (header.find("GET") == 0) {
				std::vector<std::string> get_tokens = Explode(header, " ");
				if (get_tokens.size() >= 2) {
					_resource = get_tokens[1];
				}
			}
			else {
				std::string::size_type pos = header.find(":");
				if (pos != std::string::npos) {
					std::string header_key(header, 0, pos);
					std::string header_value(header, pos + 1);
					header_value = Trim(std::move(header_value));
					if (header_key == "Host")
						_host = header_value;
					else if (header_key == "Origin")
						_origin = header_value;
					else if (header_key == "Sec-WebSocket-Key")
						_key = header_value;
					else if (header_key == "Sec-WebSocket-Protocol")
						_protocol = header_value;
				}
			}
		}

		return OPENING_FRAME;
	}

	std::string WebSocket::AnswerHandshake() {
		uint8_t digest[20]; // 160 bit sha1 digest

		std::string answer;
		answer.reserve(512);
		answer += "HTTP/1.1 101 Switching Protocols\r\n";
		answer += "Upgrade: WebSocket\r\n";
		answer += "Connection: Upgrade\r\n";

		if (_key.length() > 0) {
			std::string accept_key;
			accept_key += _key;
			accept_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; //RFC6544_MAGIC_KEY

			SHA1 sha;
			sha.Input(accept_key.data(), (uint32_t)accept_key.size());
			sha.Result((unsigned*)digest);

			//little endian to big endian
			for (int32_t i = 0; i<20; i += 4) {
				uint8_t c;

				c = digest[i];
				digest[i] = digest[i + 3];
				digest[i + 3] = c;

				c = digest[i + 1];
				digest[i + 1] = digest[i + 2];
				digest[i + 2] = c;
			}

			accept_key = base64_encode((const uint8_t *)digest, 20); //160bit = 20 bytes/chars

			answer += "Sec-WebSocket-Accept: " + (accept_key)+"\r\n";
		}

		if (_protocol.length() > 0)
			answer += "Sec-WebSocket-Protocol: " + _protocol + "\r\n";

		answer += "\r\n";

		return std::move(answer);
	}

	WebSocketFrameType WebSocket::ParseFrame(uint8_t* buff, int32_t len, Frame& frame) {
		if (len < 3)
			return INCOMPLETE_FRAME;

		uint8_t msg_opcode = buff[0] & 0x0F;
		uint8_t msg_fin = (buff[0] >> 7) & 0x01;
		uint8_t msg_masked = (buff[1] >> 7) & 0x01;

		int32_t payload_length = 0;
		int32_t pos = 2;
		int32_t length_field = buff[1] & (~0x80);
		uint32_t mask = 0;

		if (length_field <= 125) {
			payload_length = length_field;
		}
		else if (length_field == 126) { //msglen is 16bit!
			//payload_length = buff[2] + (buff[3]<<8);
			payload_length = (
				(buff[2] << 8) |
				(buff[3])
				);
			pos += 2;
		}
		else if (length_field == 127) { //msglen is 64bit!
			payload_length = (
				(buff[2] << 56) |
				(buff[3] << 48) |
				(buff[4] << 40) |
				(buff[5] << 32) |
				(buff[6] << 24) |
				(buff[7] << 16) |
				(buff[8] << 8) |
				(buff[9])
				);
			pos += 8;
		}

		if (len < payload_length + pos) {
			return INCOMPLETE_FRAME;
		}

		if (msg_masked) {
			mask = *((unsigned int*)(buff + pos));
			pos += 4;

			uint8_t* c = buff + pos;
			for (int32_t i = 0; i < payload_length; i++) {
				c[i] = c[i] ^ ((uint8_t*)(&mask))[i % 4];
			}
		}

		frame.start = (const char *)buff;
		frame.size = pos + payload_length;
		frame.payload = (const char *)buff + pos;
		frame.payloadSize = payload_length;

		if (msg_opcode == 0x0)
			return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME;
		if (msg_opcode == 0x1)
			return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME;
		if (msg_opcode == 0x2)
			return (msg_fin) ? BINARY_FRAME : INCOMPLETE_BINARY_FRAME;
		if (msg_opcode == 0x9)
			return PING_FRAME;
		if (msg_opcode == 0xA)
			return PONG_FRAME;

		return ERROR_FRAME;
	}

	void WebSocket::AnwserPongFrame(const Frame & frame) {
		uint8_t* buff = (uint8_t*)frame.start;
		buff[0] = ((buff[0] & 0xF0) | 0x0A);
		hn_send(_fd, frame.start, frame.size);
	}

	std::string WebSocket::Trim(std::string && str) {
		char* whitespace = " \t\r\n";
		std::string::size_type pos = str.find_last_not_of(whitespace);
		if (pos != std::string::npos) {
			str.erase(pos + 1);
			pos = str.find_first_not_of(whitespace);
			if (pos != std::string::npos)
				str.erase(0, pos);
		}
		else {
			return std::string();
		}
		return std::move(str);
	}

	std::vector<std::string> WebSocket::Explode(const std::string& str, const char * delimiter, bool isIncludeEmptystrings) {
		std::vector<std::string> thestringvector;
		std::string::size_type start = 0, end = 0, length = 0;
		int32_t delimiterSize = (int32_t)strlen(delimiter);

		while (end != std::string::npos) {
			end = str.find(delimiter, start);

			length = (end == std::string::npos) ? std::string::npos : end - start;

			if (isIncludeEmptystrings || ((length > 0) && (start < str.size())))
				thestringvector.push_back(str.substr(start, length));

			start = ((end >(std::string::npos - str.size())) ? std::string::npos : end + delimiterSize);
		}
		return std::move(thestringvector);
	}
}
