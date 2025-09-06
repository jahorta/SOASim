#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace simcore::tas {

	struct DtmInfo {
		std::array<char, 6> game_id{};
		bool is_wii{ false };
		uint8_t controllers{ 0 };            // 0x00B
		bool starts_from_savestate{ false }; // 0x00C
		uint64_t vi_count{ 0 };              // 0x00D
		uint64_t input_count{ 0 };           // 0x015
		uint64_t lag_count{ 0 };             // 0x01D
		uint32_t rerecord_count{ 0 };        // 0x02D
		std::array<uint8_t, 16> game_md5{};// 0x071
		uint64_t recording_start_time{ 0 };  // 0x081
		uint8_t memcard_bits{ 0 };           // 0x097
		bool memcard_blank{ false };         // 0x098
	};

	class DtmFile {
	public:
		static constexpr size_t kOffSignature = 0x000; // "DTM\x1A"
		static constexpr size_t kOffGameID = 0x004;
		static constexpr size_t kOffIsWii = 0x00A;
		static constexpr size_t kOffControllers = 0x00B;
		static constexpr size_t kOffStartsFromSavestate = 0x00C;
		static constexpr size_t kOffVICount = 0x00D;
		static constexpr size_t kOffInputCount = 0x015;
		static constexpr size_t kOffLagCount = 0x01D;
		static constexpr size_t kOffRerecordCount = 0x02D;
		static constexpr size_t kOffMD5 = 0x071;
		static constexpr size_t kOffRecordingStartTime = 0x081;
		static constexpr size_t kOffMemcardBits = 0x097;
		static constexpr size_t kOffMemcardBlank = 0x098;
		static constexpr size_t kMinHeader = 0x100;

		bool load(const std::string& path);
		bool save(const std::string& path) const;

		bool valid() const { return m_valid; }
		const std::vector<uint8_t>& bytes() const { return m_bytes; }

		DtmInfo info() const;
		void set_recording_start_time(uint64_t unix_time);

	private:
		template <class T> static T read_le(const uint8_t* p);
		template <class T> static void write_le(uint8_t* p, T v);

		std::vector<uint8_t> m_bytes;
		bool m_valid{ false };
	};

} // namespace simcore::tas
