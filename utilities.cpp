#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unistd.h>

#include "utilities.h"


std::string generateBackupFilename(std::string base) {
	auto now = std::chrono::system_clock::now();
	auto now_time_t = std::chrono::system_clock::to_time_t(now);
	auto tm = *std::localtime(&now_time_t);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
	std::string timestamp = oss.str();
	std::string filename = base + timestamp + ".backup";
	return filename;
}

std::string generateBackupFilenameWithBuildTime(std::string base) {
	std::string build_date = __DATE__; // Format: Mmm dd yyyy
	std::string build_time = __TIME__; // Format: hh:mm:ss
	std::tm build_tm = {};
	std::istringstream iss(build_date + " " + build_time);
	iss >> std::get_time(&build_tm, "%b %d %Y %H:%M:%S"); // Parse the date and time
	std::ostringstream oss;
	oss << std::put_time(&build_tm, "%Y-%m-%d_%H-%M-%S");
	std::string timestamp = oss.str();
	std::string filename = base + timestamp + ".backup";
	return filename;
}

std::error_code update_symlink(const char* target, const char* linkpath) {
	if (symlinkat(target,  AT_FDCWD, linkpath) == -1)
		return std::error_code(errno, std::generic_category());
	return std::error_code(); // Return an empty error_code, indicating success
}

std::error_code Update(TBytes newfile) {
	std::string backupFile = "/home/acces/" + generateBackupFilenameWithBuildTime("aioenetd_");
	std::error_code ec;
	std::filesystem::copy("/home/acces/aioenetd", backupFile, ec);
	if (ec) return ec;
	ec = update_symlink(backupFile.c_str(), "/etc/aioenet/aioenetd");
	if (ec)	return ec;
	std::ofstream file("/home/acces/aioenetd", std::ios::binary);
	if (!file) return std::error_code(errno, std::generic_category());
	file.write(reinterpret_cast<const char*>(newfile.data()), newfile.size());
	if (file.fail()) return std::error_code(errno, std::generic_category());
	file.close();
	if (file.fail()) return std::error_code(errno, std::generic_category());

	// TODO: Verify the new executable here

	ec = update_symlink("/home/acces/aioenetd", "/etc/aioenet/aioenetd");
	if (ec)	return ec;
	return std::error_code();
}




// #include <iostream>
// #include <functional>
// #include <chrono>
// #include <thread>
// #include <atomic>
// #include <condition_variable>

// class ResettableTimer {
// public:
//     ResettableTimer(std::chrono::milliseconds timeout, std::function<void()> callback)
//         : timeout(timeout)
//         , callback(callback)
//         , running(false)
//     { }

//     void Start() {
//         running.store(true);
//         timerThread = std::thread(&ResettableTimer::Run, this);
//     }

//     void Stop() {
//         running.store(false);
//         cv.notify_one(); // Wake up the timer thread if it's waiting
//         if(timerThread.joinable())
//             timerThread.join();
//     }

//     void Reset() {
//         std::lock_guard<std::mutex> lock(mutex);
//         reset.store(true);
//         cv.notify_one(); // Wake up the timer thread if it's waiting
//     }

// private:
//     void Run() {
//         while(running.load()) {
//             std::unique_lock<std::mutex> lock(mutex);
//             if(cv.wait_for(lock, timeout, [this] { return !running.load() || reset.load(); })) {
//                 if(reset.load()) {
//                     reset.store(false);
//                 } else {
//                     // Stop running, since we're no longer valid (not reset and not running)
//                     running.store(false);
//                 }
//             } else {
//                 callback();
//                 running.store(false); // Timeout occurred without reset, so stop running
//             }
//         }
//     }

//     std::chrono::milliseconds timeout;
//     std::function<void()> callback;
//     std::thread timerThread;
//     std::atomic<bool> running;
//     std::atomic<bool> reset;
//     std::mutex mutex;
//     std::condition_variable cv;
// };
