#include "websocket.h"
#include "sha1.h"
#include "base64.h"

namespace websocket {
	bool WebSocket::ShakeHands() {
		char msg[8192];
		int32_t pos = 0;
		int32_t len = hn_recv(_fd, msg + pos, sizeof(msg) - pos);
		while (len > 0) {
			pos += len;

			if (OPENING_FRAME != ParseHandshake((uint8_t*)msg, pos)) {

				std::string anwser = AnswerHandshake();
				hn_send(_fd, anwser.data(), anwser.size());
				return true;
			}

			len = hn_recv(_fd, msg + pos, sizeof(msg) - pos);
		}

		return false;
	}

	const char * WebSocket::ReadFrame(int32_t & size) {
		return nullptr;
	}

	void WebSocket::SendFrame(const char * data, int32_t size) {

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
		int header_end = headers.find("\r\n\r\n");

		if (header_end == std::string::npos) { // end-of-headers not found - do not parse
			return INCOMPLETE_FRAME;
		}

		headers.resize(header_end); // trim off any data we don't need after the headers
		std::vector<std::string> headers_rows = Explode(headers, "\r\n");
		for (int i = 0; i < headers_rows.size(); i++) {
			std::string& header = headers_rows[i];
			if (header.find("GET") == 0) {
				std::vector<std::string> get_tokens = Explode(header, " ");
				if (get_tokens.size() >= 2) {
					_resource = get_tokens[1];
				}
			}
			else {
				int pos = header.find(":");
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
			sha.Input(accept_key.data(), accept_key.size());
			sha.Result((unsigned*)digest);

			//little endian to big endian
			for (int i = 0; i<20; i += 4) {
				unsigned char c;

				c = digest[i];
				digest[i] = digest[i + 3];
				digest[i + 3] = c;

				c = digest[i + 1];
				digest[i + 1] = digest[i + 2];
				digest[i + 2] = c;
			}

			accept_key = base64_encode((const unsigned char *)digest, 20); //160bit = 20 bytes/chars

			answer += "Sec-WebSocket-Accept: " + (accept_key)+"\r\n";
		}

		if (_protocol.length() > 0)
			answer += "Sec-WebSocket-Protocol: " + _protocol + "\r\n";

		answer += "\r\n";

		return std::move(answer);
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
		int32_t start = 0, end = 0, length = 0;
		int32_t delimiterSize = strlen(delimiter);

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
