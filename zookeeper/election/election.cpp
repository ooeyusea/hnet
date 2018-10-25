#include "election.h"
#include "util.h"

#define NET_BUFF_SIZE 1024

void VoteSender::Start(std::string ip, int32_t port) {
	while (!_terminate) {
		_fd = hn_connect(ip.c_str(), port);
		if (!_fd) {
			//log
			hn_sleep 500;
			continue;
		}
		_activeTick = GetTickCount();

		auto test = DoWork([this] { CheckTimeout(); });
		auto pong = DoWork([this] { DealPingpong(); });

		test.Wait();
		hn_close(_fd);
		_fd = -1;

		pong.Wait();
	}
}

void VoteSender::CheckTimeout() {
	while (!_terminate) {
		hn_sleep 500;

		int64_t now = GetTickCount();
		if (now - _activeTick > 1000)
			break;
	}
}

void VoteSender::DealPingpong() {
	char buff[NET_BUFF_SIZE];
	int32_t offset = 0;
	while (true) {
		int32_t len = hn_recv(_fd, buff + offset, NET_BUFF_SIZE - offset);
		if (len < 0)
			break;

		offset += len;

		int32_t pos = 0;
		while (offset - pos >= sizeof(VoteHeader)) {
			VoteHeader& header = *(VoteHeader*)(buff + pos);
			if (offset - pos < header.size)
				break;

			if (header.id == VoteHeader::PING) {
				VoteHeader pong;
				pong.size = sizeof(VoteHeader);
				pong.id = VoteHeader::PONG;

				hn_send(_fd, (const char*)&pong, sizeof(pong));
				_activeTick = GetTickCount();
			}
			else {
				//assert(false);
			}
		}

		if (pos < offset) {
			memmove(buff, buff + pos, offset - pos);
			offset -= pos;
		}
		else
			offset = 0;
	}
}

void VoteSender::Send(const char * context, int32_t size) {
	if (_fd < 0)
		return;
	
	hn_send(_fd, context, size);
}

void VoteReciver::Start(int32_t fd, hn_channel(Vote, -1) ch) {
	_activeTick = GetTickCount();

	auto test = DoWork([this, fd] { CheckTimeout(fd); });
	auto tsp = DoWork([this, fd] { TickSendPing(fd); });
	auto np = DoWork([this, fd, ch] { DealNetPacket(fd, ch); });

	np.Wait();

	_terminate = true;
	test.Wait();
	tsp.Wait();
}

void VoteReciver::CheckTimeout(int32_t fd) {
	while (!_terminate) {
		hn_sleep 500;

		int64_t now = GetTickCount();
		if (now - _activeTick > 1000)
			break;
	}

	hn_close(fd);
}

void VoteReciver::TickSendPing(int32_t fd) {
	while (!_terminate) {
		hn_sleep 500;

		int64_t now = GetTickCount();
		if (now - _lastSendTick > 1000) {
			VoteHeader ping;
			ping.size = sizeof(VoteHeader);
			ping.id = VoteHeader::PING;

			hn_send(fd, (const char*)&ping, sizeof(ping));
		}
	}
}

void VoteReciver::DealNetPacket(int32_t fd, hn_channel(Vote, -1) ch) {
	while (true) {
		char buff[NET_BUFF_SIZE];
		int32_t offset = 0;
		while (true) {
			int32_t len = hn_recv(fd, buff + offset, NET_BUFF_SIZE - offset);
			if (len < 0)
				break;
			offset += len;

			int32_t pos = 0;
			while (offset - pos >= sizeof(VoteHeader)) {
				VoteHeader& header = *(VoteHeader*)(buff + pos);
				if (offset - pos < header.size)
					break;

				if (header.id == VoteHeader::PONG)
					_activeTick = GetTickCount();
				else if (header.id == VoteHeader::VOTE) {
					Vote& vote = *(Vote*)(buff + pos + sizeof(VoteHeader));
					ch << vote;
				}
				else {
					//assert(false);
				}
			}

			if (pos < offset) {
				memmove(buff, buff + pos, offset - pos);
				offset -= pos;
			}
			else
				offset = 0;
		}
	}
}

std::tuple<int8_t, int32_t> Election::LookForLeader() {
	while (true) {

	}
}
