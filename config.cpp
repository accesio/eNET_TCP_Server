//------------------- Configuration Files -------------
// config.cpp

#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <dirent.h>
#include <cstring>
#include <cerrno>
#include <filesystem>
namespace fs = std::filesystem;

#include "utilities.h"
#include "logging.h"
#include "apci.h"
#include "config.h"

TConfig Config;
/*
// On success: set value to config string from disk and return 0
// On error: leave value unchanged and return errno
// Config is stored in /var/lib/aioenetd/ in directories named config.factory/ config.current/ and config.user/
// in files named key.conf
static int ReadConfigString(std::string key, std::string &value, std::string which = CONFIG_CURRENT);

// On success: set value to config byte from disk and return 0
// On error: leave value unchanged and return errno
// Config is stored in /var/lib/aioenetd/ in directories named config.factory/ config.current/ and config.user/
// in files named key.conf, in two-digit HEX form; intel nybble order, padded with leading zeroes
static int ReadConfigU8(std::string key, __u8 &value, std::string which = CONFIG_CURRENT);

// On success: set value to config __u32 from disk and return 0
// On error: leave value unchanged and return errno
// Config is stored in /var/lib/aioenetd/ in directories named config.factory/ config.current/ and config.user/
// in files named key.conf, in 8-digit HEX form; intel order, padded with leading zeroes
static int ReadConfigU32(std::string key, __u32 &value, std::string which = CONFIG_CURRENT);

// On success: set value to config float from disk and return 0
// On error: leave value unchanged and return errno
// Config is stored in /var/lib/aioenetd/ in directories named config.factory/ config.current/ and config.user/
// in files named key.conf, in 8-digit HEX form; intel order, padded with leading zeroes
// (the single-precision 4-byte float is stored as a __u32 for perfect round-trip serdes)
static int ReadConfigFloat(std::string key, float &value, std::string which = CONFIG_CURRENT);

static int WriteConfigString(std::string key, std::string value, std::string which = CONFIG_CURRENT);
static int WriteConfigFloat(std::string key, float value, std::string which = CONFIG_CURRENT);
static int WriteConfigU8(std::string key, __u8 value, std::string which = CONFIG_CURRENT);
static int WriteConfigU32(std::string key, __u32 value, std::string which = CONFIG_CURRENT);
*/

static inline int FsyncDir(const std::string &dirPath)
{
	int dfd = open(dirPath.c_str(), O_DIRECTORY | O_CLOEXEC);
	if (dfd < 0)
		return -errno;
	int rc = fsync(dfd);
	int e = (rc == 0) ? 0 : -errno;
	close(dfd);
	return e;
}

static int MkdirP(const std::string &path, mode_t mode)
{
	std::error_code ec;
	if (fs::exists(path, ec))
		return 0;
	if (!fs::create_directories(path, ec))
	{
		if (ec)
			return -ec.value();
	}
	// best-effort: fsync each level (parent is enough in practice)
	return FsyncDir(fs::path(path).parent_path().string());
}

static int AtomicWriteTextFile(const std::string &path, const std::string &contents, mode_t mode)
{
	std::string tmp = path + ".tmp";
	int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
	if (fd < 0)
	{
		int e = -errno;
		Error("AtomicWriteTextFile open(" + tmp + ") failed: " + std::to_string(-e));
		return e;
	}
	ssize_t need = static_cast<ssize_t>(contents.size());
	const char *p = contents.data();
	while (need > 0)
	{
		ssize_t w = write(fd, p, need);
		if (w < 0)
		{
			int e = -errno;
			Error("AtomicWriteTextFile write(" + tmp + ") failed: " + std::to_string(-e));
			close(fd);
			return e;
		}
		need -= w;
		p += w;
	}
	if (fsync(fd) != 0)
	{
		int e = -errno;
		Error("AtomicWriteTextFile fsync(" + tmp + ") failed: " + std::to_string(-e));
		close(fd);
		return e;
	}
	close(fd);

	// fsync parent dir to persist rename
	auto parent = fs::path(path).parent_path().string();
	if (rename(tmp.c_str(), path.c_str()) != 0)
	{
		int e = -errno;
		Error("AtomicWriteTextFile rename(" + tmp + " -> " + path + ") failed: " + std::to_string(-e));
		return e;
	}
	(void)FsyncDir(parent);
	return 0;
}

static inline std::string FilePathFor(const std::string &which, const std::string &key)
{
	// (CONFIG_PATH already ends with '/'; which ends with '/')
	return std::string(CONFIG_PATH) + which + key + ".conf";
}

bool EnsureConfigTree()
{
	// /var/share/aioenetd/
	if (int rc = MkdirP(CONFIG_PATH, 0755); rc < 0)
		return false;
	if (int rc = MkdirP(std::string(CONFIG_PATH) + CONFIG_FACTORY, 0755); rc < 0)
		return false;
	if (int rc = MkdirP(std::string(CONFIG_PATH) + CONFIG_CURRENT, 0755); rc < 0)
		return false;
	if (int rc = MkdirP(std::string(CONFIG_PATH) + CONFIG_USER, 0755); rc < 0)
		return false;
	return true;
}

static bool IsFirstBoot()
{
	std::error_code ec;
	return !fs::exists(std::string(CONFIG_PATH) + CONFIG_INIT_STAMP, ec);
}

static bool WriteInitStamp()
{
	std::string stampPath = std::string(CONFIG_PATH) + CONFIG_INIT_STAMP;
	return AtomicWriteTextFile(stampPath, AIOENETD_VERSION, 0644) == 0;
}

// write a key to a subtree iff missing (factory/current/user)
static int WriteIfMissing(const std::string &which, const std::string &key, const std::string &value)
{
	std::error_code ec;
	std::string path = FilePathFor(which, key);
	if (fs::exists(path, ec))
		return 0; // present; nothing to do
	return AtomicWriteTextFile(path, value, 0644);
}

// helpers to dump Config into a target subtree, but only if that key is missing
static void SeedFactoryFromStruct(const TConfig &cfg)
{
	// Board info
	WriteIfMissing(CONFIG_FACTORY, "BRD_Description", cfg.Description);
	WriteIfMissing(CONFIG_FACTORY, "BRD_Model", cfg.Model);
	WriteIfMissing(CONFIG_FACTORY, "BRD_SerialNumber", cfg.SerialNumber);

	// ADC ranges
	WriteIfMissing(CONFIG_FACTORY, "ADC_Differential", to_hex<__u8>(cfg.adcDifferential));
	for (int i = 0; i < 16; i++)
	{
		char key[32];
		snprintf(key, sizeof(key), "ADC_RangeCodeCh%02d", i);
		WriteIfMissing(CONFIG_FACTORY, key, to_hex<__u32>(cfg.adcRangeCodes[i]));
	}

	// ADC cal (hex-IEEE float)
	for (int r = 0; r < 8; r++)
	{
		char ks[32];
		snprintf(ks, sizeof(ks), "ADC_ScaleRange%d", r);
		char ko[32];
		snprintf(ko, sizeof(ko), "ADC_OffsetRange%d", r);
		WriteIfMissing(CONFIG_FACTORY, ks, to_hex<__u32>(bit_cast<__u32>(cfg.adcScaleCoefficients[r])));
		WriteIfMissing(CONFIG_FACTORY, ko, to_hex<__u32>(bit_cast<__u32>(cfg.adcOffsetCoefficients[r])));
	}

	WriteIfMissing(CONFIG_FACTORY, "DAC_NumDACs", to_hex<__u8>(cfg.NUM_DACs));

	// DAC ranges
	for (int i = 0; i < MAX_DACs; i++)
	{
		char key[32];
		snprintf(key, sizeof(key), "DAC_RangeCh%d", i);
		WriteIfMissing(CONFIG_FACTORY, key, to_hex<__u32>(cfg.dacRanges[i]));
	}

	// DAC cal
	for (int d = 0; d < MAX_DACs; d++)
	{
		char ks[32];
		snprintf(ks, sizeof(ks), "DAC_ScaleCh%d", d);
		char ko[32];
		snprintf(ko, sizeof(ko), "DAC_OffsetCh%d", d);
		WriteIfMissing(CONFIG_FACTORY, ks, to_hex<__u32>(bit_cast<__u32>(cfg.dacScaleCoefficients[d])));
		WriteIfMissing(CONFIG_FACTORY, ko, to_hex<__u32>(bit_cast<__u32>(cfg.dacOffsetCoefficients[d])));
	}

	// Submux
	WriteIfMissing(CONFIG_FACTORY, "BRD_NumberOfSubmuxes", to_hex<__u8>(cfg.numberOfSubmuxes));
	for (int i = 0; i < 4; i++)
	{
		char kb[32];
		snprintf(kb, sizeof(kb), "BRD_SubmuxBarcode%d", i);
		char kt[32];
		snprintf(kt, sizeof(kt), "BRD_SubmuxType%d", i);
		WriteIfMissing(CONFIG_FACTORY, kb, cfg.submuxBarcodes[i]);
		WriteIfMissing(CONFIG_FACTORY, kt, cfg.submuxTypes[i]);
		for (int g = 0; g < gainGroupsPerSubmux; g++)
		{
			char ksf[32];
			snprintf(ksf, sizeof(ksf), "BRD_Submux%dRange%d", i, g);
			char kof[32];
			snprintf(kof, sizeof(kof), "BRD_Submux%dOffset%d", i, g);
			WriteIfMissing(CONFIG_FACTORY, ksf, to_hex<__u32>(bit_cast<__u32>(cfg.submuxScaleFactors[i][g])));
			WriteIfMissing(CONFIG_FACTORY, kof, to_hex<__u32>(bit_cast<__u32>(cfg.submuxOffsets[i][g])));
		}
	}
}

// copy any factory key (if exists) into current if missing there
static void SeedCurrentFromFactoryIfMissing()
{
	// simply iterate factory dir and copy any missing files into current
	const std::string facDir = std::string(CONFIG_PATH) + CONFIG_FACTORY;
	const std::string curDir = std::string(CONFIG_PATH) + CONFIG_CURRENT;

	std::error_code ec;
	if (!fs::exists(facDir, ec))
		return;
	for (auto &entry : fs::directory_iterator(facDir, ec))
	{
		if (!entry.is_regular_file())
			continue;
		auto name = entry.path().filename().string(); // e.g. Foo.conf
		if (name.size() < 6 || name.rfind(".conf") != name.size() - 5)
			continue;
		auto dst = curDir + name;
		if (!fs::exists(dst, ec))
		{
			// read whole file then AtomicWrite
			std::ifstream in(entry.path(), std::ios::binary);
			std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			AtomicWriteTextFile(dst, contents, 0644);
		}
	}
}

void InitializeConfigFiles(TConfig &config)
{
	if (!EnsureConfigTree())
	{
		Error("InitializeConfigFiles: failed EnsureConfigTree()");
		return;
	}
	if (!IsFirstBoot())
	{
		Debug("InitializeConfigFiles: already initialized; skipping seeding");
		return;
	}
	// seed factory from compiled defaults (Config was pre-initialized by InitConfig)
	SeedFactoryFromStruct(config);
	// seed current from factory where missing
	SeedCurrentFromFactoryIfMissing();
	// future: optionally seed a default named user profile

	if (!WriteInitStamp())
	{
		Error("InitializeConfigFiles: failed to write init stamp");
	}
	else
	{
		Log("InitializeConfigFiles: initial config written; stamp recorded: " AIOENETD_VERSION);
	}
}

/* There are 16 ADC ranges, one per base board channel
   There can be one submux (AIMUX64M) attached, which turns each ADC channel of the base board into 4 more channels (64 total),
   or up to four submux (AIMUX32), which turn groups of four ADC channels into 32 channels. With the AIMUX32 each ADC channel's group
   of 8 submux channels can have complex signal conditioning features applied.  The AIMUX64M merely allows an additional gain factor.
   This Config struct allows per-submux-channel-group calibration and scaling, as well as the range and calibration for the four DACs and ADC.
*/
void InitConfig(TConfig &config)
{
	config.Hostname = "enetaio000000000000000";
	config.Model = "eNET-untested";
	config.Description = "eNET-untested Description";
	config.SerialNumber = "eNET-unset Serial Number";
	config.FpgaVersionCode = 0xDEADBA57;
	config.numberOfSubmuxes = 0;
	config.adcDifferential = 0b00000000;
	config.NUM_DACs = 2;
	for (int i = 0; i < 16; i++)
	{
		if (i < 4)
		{
			config.submuxBarcodes[i] = "";
			config.submuxTypes[i] = "";
			for (int submux = 0; submux < 4; submux++)
			{
				config.submuxScaleFactors[i][submux] = 1.0;
				config.submuxOffsets[i][submux] = 0.0;
			}
			config.dacRanges[i] = 0x00000001;
			config.dacScaleCoefficients[i] = 1.00000000;
			config.dacOffsetCoefficients[i] = 0.0;
		}
		if (i < 8)
		{
			config.adcScaleCoefficients[i] = 1.0;
			config.adcOffsetCoefficients[i] = 0.0;
		}
		if (i < 16)
		{
			config.adcRangeCodes[i] = 1;
		}
	}
}

static int ReadConfigStringRaw(std::string key, std::string &value, std::string which)
{
	std::string path = FilePathFor(which, key);
	int f = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (f < 0)
	{
		int e = -errno;
		// Missing is normal (fallback will try next layer) -> no log
		if (e == -ENOENT || e == -ENOTDIR)
			return -ENOENT;

		Error("ReadConfigString open(" + path + ") failed: " + std::to_string(-e));
		return e;
	}

	char buf[1024];
	ssize_t r = read(f, buf, sizeof(buf) - 1);
	int ret;
	if (r < 0)
	{
		ret = -errno;
		// EISDIR/ENOENT here would be odd, but still: only log unexpected errors
		if (!(ret == -ENOENT || ret == -ENOTDIR))
		{
			Error("ReadConfigString read(" + path + ") failed: " + std::to_string(-ret));
		}
	}
	else
	{
		buf[r] = 0;
		value.assign(buf);
		while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
			value.pop_back();
		ret = 0;
	}
	close(f);
	return ret;
}

int ReadConfigString(const std::string &key, std::string &value, const std::string &which)
{
	std::string path = FilePathFor(which, key);
	std::error_code ec;
	if (fs::exists(path, ec))
		return ReadConfigStringRaw(key, value, which);

	if (which.rfind("config.user/", 0) == 0)
	{
		if (fs::exists(FilePathFor(CONFIG_CURRENT, key), ec))
		{
			return ReadConfigStringRaw(key, value, CONFIG_CURRENT);
		}
	}
	// final fallback to factory
	if (fs::exists(FilePathFor(CONFIG_FACTORY, key), ec))
	{
		return ReadConfigStringRaw(key, value, CONFIG_FACTORY);
	}
	return -ENOENT;
}

int ReadConfigU8(std::string key, __u8 &value, std::string which)
{
	std::string v;
	int rc = ReadConfigString(key, v, which);
	if (rc < 0)
		return rc;
	if (v.size() < 2)
		return -EINVAL;
	value = static_cast<__u8>(std::stoul(v, nullptr, 16));
	return 0;
}

int ReadConfigU32(std::string key, __u32 &value, std::string which)
{
	std::string v;
	int rc = ReadConfigString(key, v, which);
	if (rc < 0)
		return rc;
	if (v.size() < 8)
		return -EINVAL;
	value = static_cast<__u32>(std::stoul(v, nullptr, 16));
	return 0;
}

int ReadConfigFloat(std::string key, float &value, std::string which)
{
	__u32 u = 0;
	int rc = ReadConfigU32(key, u, which);
	if (rc < 0)
		return rc;
	value = bit_cast<float>(u);
	return 0;
}

int WriteConfigString(std::string key, std::string value, std::string which)
{
	std::string path = FilePathFor(which, key);
	// ensure dir exists (idempotent)
	MkdirP(fs::path(path).parent_path().string(), 0755);
	int rc = AtomicWriteTextFile(path, value, 0644);
	if (rc < 0)
	{
		Error("WriteConfigString(" + path + ") failed: " + std::to_string(-rc));
	}
	return rc;
}

int WriteConfigU8(std::string key, __u8 value, std::string which)
{
	std::string v = to_hex<__u8>(value);
	return WriteConfigString(key, v, which);
}

int WriteConfigU32(std::string key, __u32 value, std::string which)
{
	std::string v = to_hex<__u32>(value);
	return WriteConfigString(key, v, which);
}

int WriteConfigFloat(std::string key, float value, std::string which)
{
	// map_f__u32.f = value;
	__u32 v = bit_cast<__u32>(value);
	return WriteConfigU32(key, v, which);
	// std::cout << "	 wrote "+key+" as with "<< value <<" during WriteConfigFloat"<<'\n';
}

// TODO: improve error handling program-wide
#define HandleError(x)      \
	{                       \
		if (x < 0)          \
			Error("Error"); \
	}

/* LOAD CONFIGURATION STRUCT FROM DISK FILES */
void LoadDacCalConfig(std::string which)
{
	HandleError(ReadConfigFloat("DAC_ScaleCh0", Config.dacScaleCoefficients[0], which));
	HandleError(ReadConfigFloat("DAC_ScaleCh1", Config.dacScaleCoefficients[1], which));
	HandleError(ReadConfigFloat("DAC_ScaleCh2", Config.dacScaleCoefficients[2], which));
	HandleError(ReadConfigFloat("DAC_ScaleCh3", Config.dacScaleCoefficients[3], which));

	HandleError(ReadConfigFloat("DAC_OffsetCh0", Config.dacOffsetCoefficients[0], which));
	HandleError(ReadConfigFloat("DAC_OffsetCh1", Config.dacOffsetCoefficients[1], which));
	HandleError(ReadConfigFloat("DAC_OffsetCh2", Config.dacOffsetCoefficients[2], which));
	HandleError(ReadConfigFloat("DAC_OffsetCh3", Config.dacOffsetCoefficients[3], which));
}
void LoadAdcCalConfig(std::string which)
{
	HandleError(ReadConfigFloat("ADC_ScaleRange0", Config.adcScaleCoefficients[0], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange1", Config.adcScaleCoefficients[1], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange2", Config.adcScaleCoefficients[2], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange3", Config.adcScaleCoefficients[3], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange4", Config.adcScaleCoefficients[4], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange5", Config.adcScaleCoefficients[5], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange6", Config.adcScaleCoefficients[6], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange7", Config.adcScaleCoefficients[7], which));

	HandleError(ReadConfigFloat("ADC_OffsetRange0", Config.adcOffsetCoefficients[0], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange1", Config.adcOffsetCoefficients[1], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange2", Config.adcOffsetCoefficients[2], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange3", Config.adcOffsetCoefficients[3], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange4", Config.adcOffsetCoefficients[4], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange5", Config.adcOffsetCoefficients[5], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange6", Config.adcOffsetCoefficients[6], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange7", Config.adcOffsetCoefficients[7], which));
}

void LoadCalConfig(std::string which)
{
	LoadDacCalConfig(which);
	LoadAdcCalConfig(which);
}

void LoadDacConfig(std::string which)
{
	HandleError(ReadConfigU32("DAC_RangeCh0", Config.dacRanges[0], which));
	HandleError(ReadConfigU32("DAC_RangeCh1", Config.dacRanges[1], which));
	HandleError(ReadConfigU32("DAC_RangeCh2", Config.dacRanges[2], which));
	HandleError(ReadConfigU32("DAC_RangeCh3", Config.dacRanges[3], which));
}

void LoadAdcConfig(std::string which)
{
	HandleError(ReadConfigU8("ADC_Differential", Config.adcDifferential, which));

	HandleError(ReadConfigU32("ADC_RangeCodeCh00", Config.adcRangeCodes[0], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh01", Config.adcRangeCodes[1], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh02", Config.adcRangeCodes[2], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh03", Config.adcRangeCodes[3], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh04", Config.adcRangeCodes[4], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh05", Config.adcRangeCodes[5], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh06", Config.adcRangeCodes[6], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh07", Config.adcRangeCodes[7], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh08", Config.adcRangeCodes[8], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh09", Config.adcRangeCodes[9], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh10", Config.adcRangeCodes[10], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh11", Config.adcRangeCodes[11], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh12", Config.adcRangeCodes[12], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh13", Config.adcRangeCodes[13], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh14", Config.adcRangeCodes[14], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh15", Config.adcRangeCodes[15], which));
}

void LoadSubmuxConfig(std::string which)
{
	HandleError(ReadConfigU8("BRD_NumberOfSubmuxes", Config.numberOfSubmuxes, which));

	HandleError(ReadConfigString("BRD_SubmuxBarcode0", Config.submuxBarcodes[0], which));
	HandleError(ReadConfigString("BRD_SubmuxBarcode1", Config.submuxBarcodes[1], which));
	HandleError(ReadConfigString("BRD_SubmuxBarcode2", Config.submuxBarcodes[2], which));
	HandleError(ReadConfigString("BRD_SubmuxBarcode3", Config.submuxBarcodes[3], which));

	HandleError(ReadConfigString("BRD_SubmuxType0", Config.submuxTypes[0], which));
	HandleError(ReadConfigString("BRD_SubmuxType1", Config.submuxTypes[1], which));
	HandleError(ReadConfigString("BRD_SubmuxType2", Config.submuxTypes[2], which));
	HandleError(ReadConfigString("BRD_SubmuxType3", Config.submuxTypes[3], which));

	HandleError(ReadConfigFloat("BRD_Submux0Range0", Config.submuxScaleFactors[0][0], which));
	HandleError(ReadConfigFloat("BRD_Submux0Range1", Config.submuxScaleFactors[0][1], which));
	HandleError(ReadConfigFloat("BRD_Submux0Range2", Config.submuxScaleFactors[0][2], which));
	HandleError(ReadConfigFloat("BRD_Submux0Range3", Config.submuxScaleFactors[0][3], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range0", Config.submuxScaleFactors[1][0], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range1", Config.submuxScaleFactors[1][1], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range2", Config.submuxScaleFactors[1][2], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range3", Config.submuxScaleFactors[1][3], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range0", Config.submuxScaleFactors[2][0], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range1", Config.submuxScaleFactors[2][1], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range2", Config.submuxScaleFactors[2][2], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range3", Config.submuxScaleFactors[2][3], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range0", Config.submuxScaleFactors[3][0], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range1", Config.submuxScaleFactors[3][1], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range2", Config.submuxScaleFactors[3][2], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range3", Config.submuxScaleFactors[3][3], which));

	HandleError(ReadConfigFloat("BRD_Submux0Offset0", Config.submuxOffsets[0][0], which));
	HandleError(ReadConfigFloat("BRD_Submux0Offset1", Config.submuxOffsets[0][1], which));
	HandleError(ReadConfigFloat("BRD_Submux0Offset2", Config.submuxOffsets[0][2], which));
	HandleError(ReadConfigFloat("BRD_Submux0Offset3", Config.submuxOffsets[0][3], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset0", Config.submuxOffsets[1][0], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset1", Config.submuxOffsets[1][1], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset2", Config.submuxOffsets[1][2], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset3", Config.submuxOffsets[1][3], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset0", Config.submuxOffsets[2][0], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset1", Config.submuxOffsets[2][1], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset2", Config.submuxOffsets[2][2], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset3", Config.submuxOffsets[2][3], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset0", Config.submuxOffsets[3][0], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset1", Config.submuxOffsets[3][1], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset2", Config.submuxOffsets[3][2], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset3", Config.submuxOffsets[3][3], which));
}

// reads some BRD_ data from config files
void LoadBrdConfig(std::string which)
{
	HandleError(ReadConfigString("BRD_Model", Config.Model, which));
	HandleError(ReadConfigString("BRD_Description", Config.Description, which));
	HandleError(ReadConfigString("BRD_SerialNumber", Config.SerialNumber, which));
}

// Reads the /var/lib/aioenetd/config.current/configuration data into the Config structure
void LoadConfig(std::string which)
{
	{
		// read /etc/hostname into Config.Hostname
		std::ifstream in("/etc/hostname");
		std::getline(in, Config.Hostname);
		in.close();
		Debug("Hostname == " + Config.Hostname);
	}

	LoadBrdConfig(which);
	LoadCalConfig(which);
	LoadDacConfig(which);
	LoadAdcConfig(which);
	LoadSubmuxConfig(which);
}

/* SAVE CONFIGURATION STRUCT TO DISK */
bool SaveDacCalConfig(std::string which)
{
	HandleError(WriteConfigFloat("DAC_ScaleCh0", Config.dacScaleCoefficients[0], which));
	HandleError(WriteConfigFloat("DAC_ScaleCh1", Config.dacScaleCoefficients[1], which));
	HandleError(WriteConfigFloat("DAC_ScaleCh2", Config.dacScaleCoefficients[2], which));
	HandleError(WriteConfigFloat("DAC_ScaleCh3", Config.dacScaleCoefficients[3], which));

	HandleError(WriteConfigFloat("DAC_OffsetCh0", Config.dacOffsetCoefficients[0], which));
	HandleError(WriteConfigFloat("DAC_OffsetCh1", Config.dacOffsetCoefficients[1], which));
	HandleError(WriteConfigFloat("DAC_OffsetCh2", Config.dacOffsetCoefficients[2], which));
	HandleError(WriteConfigFloat("DAC_OffsetCh3", Config.dacOffsetCoefficients[3], which));
	return true;
}

bool SaveAdcCalConfig(std::string which)
{
	HandleError(WriteConfigFloat("ADC_ScaleRange0", Config.adcScaleCoefficients[0], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange1", Config.adcScaleCoefficients[1], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange2", Config.adcScaleCoefficients[2], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange3", Config.adcScaleCoefficients[3], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange4", Config.adcScaleCoefficients[4], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange5", Config.adcScaleCoefficients[5], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange6", Config.adcScaleCoefficients[6], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange7", Config.adcScaleCoefficients[7], which));

	HandleError(WriteConfigFloat("ADC_OffsetRange0", Config.adcOffsetCoefficients[0], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange1", Config.adcOffsetCoefficients[1], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange2", Config.adcOffsetCoefficients[2], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange3", Config.adcOffsetCoefficients[3], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange4", Config.adcOffsetCoefficients[4], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange5", Config.adcOffsetCoefficients[5], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange6", Config.adcOffsetCoefficients[6], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange7", Config.adcOffsetCoefficients[7], which));
	return true;
}

bool SaveCalConfig(std::string which)
{
	SaveDacCalConfig(which);
	SaveAdcCalConfig(which);
	return true;
}

bool SaveDacConfig(std::string which)
{
	HandleError(WriteConfigU32("DAC_RangeCh0", Config.dacRanges[0], which));
	HandleError(WriteConfigU32("DAC_RangeCh1", Config.dacRanges[1], which));
	HandleError(WriteConfigU32("DAC_RangeCh2", Config.dacRanges[2], which));
	HandleError(WriteConfigU32("DAC_RangeCh3", Config.dacRanges[3], which));
	return true;
}

bool SaveAdcConfig(std::string which)
{
	HandleError(WriteConfigU8("ADC_Differential", Config.adcDifferential, which));

	HandleError(WriteConfigU32("ADC_RangeCodeCh00", Config.adcRangeCodes[0], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh01", Config.adcRangeCodes[1], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh02", Config.adcRangeCodes[2], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh03", Config.adcRangeCodes[3], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh04", Config.adcRangeCodes[4], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh05", Config.adcRangeCodes[5], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh06", Config.adcRangeCodes[6], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh07", Config.adcRangeCodes[7], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh08", Config.adcRangeCodes[8], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh09", Config.adcRangeCodes[9], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh10", Config.adcRangeCodes[10], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh11", Config.adcRangeCodes[11], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh12", Config.adcRangeCodes[12], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh13", Config.adcRangeCodes[13], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh14", Config.adcRangeCodes[14], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh15", Config.adcRangeCodes[15], which));
	return true;
}

bool SaveSubmuxConfig(std::string which)
{
	HandleError(WriteConfigU8("BRD_NumberOfSubmuxes", Config.numberOfSubmuxes, which));

	HandleError(WriteConfigString("BRD_SubmuxBarcode0", Config.submuxBarcodes[0], which));
	HandleError(WriteConfigString("BRD_SubmuxBarcode1", Config.submuxBarcodes[1], which));
	HandleError(WriteConfigString("BRD_SubmuxBarcode2", Config.submuxBarcodes[2], which));
	HandleError(WriteConfigString("BRD_SubmuxBarcode3", Config.submuxBarcodes[3], which));

	HandleError(WriteConfigString("BRD_SubmuxType0", Config.submuxTypes[0], which));
	HandleError(WriteConfigString("BRD_SubmuxType1", Config.submuxTypes[1], which));
	HandleError(WriteConfigString("BRD_SubmuxType2", Config.submuxTypes[2], which));
	HandleError(WriteConfigString("BRD_SubmuxType3", Config.submuxTypes[3], which));

	HandleError(WriteConfigFloat("BRD_Submux0Range0", Config.submuxScaleFactors[0][0], which));
	HandleError(WriteConfigFloat("BRD_Submux0Range1", Config.submuxScaleFactors[0][1], which));
	HandleError(WriteConfigFloat("BRD_Submux0Range2", Config.submuxScaleFactors[0][2], which));
	HandleError(WriteConfigFloat("BRD_Submux0Range3", Config.submuxScaleFactors[0][3], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range0", Config.submuxScaleFactors[1][0], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range1", Config.submuxScaleFactors[1][1], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range2", Config.submuxScaleFactors[1][2], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range3", Config.submuxScaleFactors[1][3], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range0", Config.submuxScaleFactors[2][0], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range1", Config.submuxScaleFactors[2][1], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range2", Config.submuxScaleFactors[2][2], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range3", Config.submuxScaleFactors[2][3], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range0", Config.submuxScaleFactors[3][0], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range1", Config.submuxScaleFactors[3][1], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range2", Config.submuxScaleFactors[3][2], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range3", Config.submuxScaleFactors[3][3], which));

	HandleError(WriteConfigFloat("BRD_Submux0Offset0", Config.submuxOffsets[0][0], which));
	HandleError(WriteConfigFloat("BRD_Submux0Offset1", Config.submuxOffsets[0][1], which));
	HandleError(WriteConfigFloat("BRD_Submux0Offset2", Config.submuxOffsets[0][2], which));
	HandleError(WriteConfigFloat("BRD_Submux0Offset3", Config.submuxOffsets[0][3], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset0", Config.submuxOffsets[1][0], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset1", Config.submuxOffsets[1][1], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset2", Config.submuxOffsets[1][2], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset3", Config.submuxOffsets[1][3], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset0", Config.submuxOffsets[2][0], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset1", Config.submuxOffsets[2][1], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset2", Config.submuxOffsets[2][2], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset3", Config.submuxOffsets[2][3], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset0", Config.submuxOffsets[3][0], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset1", Config.submuxOffsets[3][1], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset2", Config.submuxOffsets[3][2], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset3", Config.submuxOffsets[3][3], which));
	return true;
}

bool SaveBrdConfig(std::string which)
{
	HandleError(WriteConfigString("BRD_Description", Config.Description, which));
	HandleError(WriteConfigString("BRD_Model", Config.Model, which));
	HandleError(WriteConfigString("BRD_SerialNumber", Config.SerialNumber, which));
	return true;
}

// saves the active configuration into current.config or which
bool SaveConfig(std::string which)
{
	Debug("SaveConfig writing Config struct to config files " + which);

	SaveBrdConfig(which);
	SaveCalConfig(which);
	SaveDacConfig(which);
	SaveAdcConfig(which);
	SaveSubmuxConfig(which);

	return true;
}

void ApplyAdcCalConfig()
{
	for (int cal = 0; cal < 8; ++cal)
	{
		out(ofsAdcCalScale + cal * ofsAdcCalScaleStride, *reinterpret_cast<__u32 *>(&Config.adcScaleCoefficients[cal]));
		out(ofsAdcCalOffset + cal * ofsAdcCalOffsetStride, *reinterpret_cast<__u32 *>(&Config.adcOffsetCoefficients[cal]));
	}
}

void ApplyConfig()
{
	ApplyAdcCalConfig();
}