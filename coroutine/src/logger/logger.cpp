#include "logger.h"
#include "hnet.h"
#include "spdlog/logger.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "options.h"
#include "spin_mutex.h"
#include <string>

#define DAY_INTERVAL 24 * 3600 * 1000

std::shared_ptr<spdlog::logger> g_logger;

namespace hyper_net {
	struct console_mutex {
		using mutex_t = spin_mutex;

		static mutex_t &mutex() {
			static mutex_t s_mutex;
			return s_mutex;
		}
	};

#ifdef WIN32
	using StdoutColorSink = spdlog::sinks::wincolor_sink<spdlog::details::console_stdout, console_mutex>;
#else
	using StdoutColorSink = spdlog::sinks::ansicolor_sink<spdlog::details::console_stdout, console_mutex>;
#endif

	template<typename Mutex>
	class DailyRotatingFileSink final : public spdlog::sinks::base_sink<Mutex> {
	public:
		DailyRotatingFileSink(const std::string& baseFilename, int32_t maxSize)
			: _baseFilename(baseFilename) 
			, _maxSize(maxSize) {

			OpenFile();
		}

	protected:
		void sink_it_(const spdlog::details::log_msg &msg) override {
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time - _rotationTp);
			if (duration.count() > DAY_INTERVAL) {
				_nextIdx = 0;
				Rotate(msg.time);
				_rotationTp = NextRotationTp();
				_currentSize = 0;
			}

			fmt::memory_buffer formatted;
			spdlog::sinks::sink::formatter_->format(msg, formatted);
			_currentSize += formatted.size();
			if (_currentSize > _maxSize) {
				Rotate(msg.time);
				_currentSize = (int32_t)formatted.size();
			}
			_fileHelper.write(formatted);
		}

		void flush_() override {
			_fileHelper.flush();
		}

	private:
		inline tm NowTm(std::chrono::system_clock::time_point tp) {
			time_t tnow = std::chrono::system_clock::to_time_t(tp);
			return spdlog::details::os::localtime(tnow);
		}

		inline std::chrono::system_clock::time_point NextRotationTp() {
			auto now = std::chrono::system_clock::now();

			tm date = NowTm(now);
			date.tm_hour = 0;
			date.tm_min = 0;
			date.tm_sec = 0;

			auto rotationTime = std::chrono::system_clock::from_time_t(std::mktime(&date));
			return { rotationTime + std::chrono::hours(24) };
		}

		inline void OpenFile() {
			auto now = std::chrono::system_clock::now();
			_fileHelper.open(CalcFilename(NowTm(now)), false);
			_rotationTp = NextRotationTp();
		}

		inline void Rotate(std::chrono::system_clock::time_point tp) {
			_fileHelper.open(CalcFilename(NowTm(tp)), false);
		}

		inline std::string CalcFilename(const std::string &filename, tm now, int32_t index) {
			std::string basename, ext;
			std::tie(basename, ext) = spdlog::details::file_helper::split_by_extenstion(filename);

			fmt::memory_buffer w;
			if (index != 0u) {
				fmt::format_to(w, SPDLOG_FILENAME_T("{}_{:04d}{:02d}{:02d}.{}{}"), basename, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, index, ext);
			}
			else
				fmt::format_to(w, SPDLOG_FILENAME_T("{}_{:04d}{:02d}{:02d}{}"), basename, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, ext);

			return fmt::to_string(w);
		}

		inline std::string CalcFilename(tm now) {
			while (true) {
				std::string filename = CalcFilename(_baseFilename, now, _nextIdx++);
				if (!spdlog::details::os::file_exists(filename))
					return filename;
			}
		}

	private:
		spdlog::details::file_helper _fileHelper;

		int32_t _currentSize = 0;
		int32_t _maxSize = 0;
		std::string _baseFilename;

		std::chrono::system_clock::time_point _rotationTp;
		int32_t _nextIdx = 0;
	};

	bool InitLogger() {
		try {
			spdlog::init_thread_pool(Options::Instance().GetLoggerQueueSize(), Options::Instance().GetLogThread());

			spdlog::level::level_enum l;
			std::vector<spdlog::sink_ptr> sinks;
			if (Options::Instance().IsLogConsole()) {
				sinks.push_back(std::make_shared<StdoutColorSink>());
				sinks[0]->set_level(spdlog::level::trace);
				l = spdlog::level::trace;
			}
			else
				l = (spdlog::level::level_enum)Options::Instance().GetLoggerLevel();

			std::string filename = "log/";
			filename += Options::Instance().GetExeName();
			filename += ".log";

			sinks.push_back(std::make_shared<DailyRotatingFileSink<spin_mutex>>(filename, Options::Instance().GetLoggerFileSize()));
			(*sinks.rbegin())->set_level((spdlog::level::level_enum)Options::Instance().GetLoggerLevel());

			g_logger = std::make_shared<spdlog::async_logger>("lg", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			g_logger->set_pattern("[%H:%M:%S.%e][%l]%v");
			g_logger->set_level(l);
		}
		catch (spdlog::spdlog_ex& e) {
			printf("%s\n", e.what());
			return false;
		}
		return true;
	}
}
