#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../driver_memory.hpp"
#include "debug/Logger.hpp"

enum class TransportStatus {
	Ok,
	NotReady,
	Detached,
	ReadFailed,
	WriteFailed,
	Timeout,
	InvalidTarget
};

inline const char* TransportStatusName(TransportStatus status) noexcept
{
	switch (status) {
	case TransportStatus::Ok: return "Ok";
	case TransportStatus::NotReady: return "NotReady";
	case TransportStatus::Detached: return "Detached";
	case TransportStatus::ReadFailed: return "ReadFailed";
	case TransportStatus::WriteFailed: return "WriteFailed";
	case TransportStatus::Timeout: return "Timeout";
	case TransportStatus::InvalidTarget: return "InvalidTarget";
	}
	return "Unknown";
}

inline void LogTransportStatus(TransportStatus status) noexcept
{
	if (status == TransportStatus::Ok)
		return;
	Logger_TransportError(TransportStatusName(status));
}

inline bool validpointer(std::uintptr_t pointer) noexcept
{
	if (!pointer) return false;
	// Alignment check removed: Bugbot flagged validpointer() as needing alignment
	// validation to match is_valid_pointer_optimized() (commit 1690b5d). Testing
	// proved this was wrong for this codebase's pointer domain — some pointers in
	// the FName resolution chain (GNames chunk pointers, name entry pointers,
	// string data pointers) are legitimately not 8-byte aligned in ARC Raiders
	// memory. Enforcing alignment here broke name resolution entirely.
	// Keep validation limited to null check and user-mode range check.
	return pointer >= 0x10000ULL && pointer <= 0x00007FFFFFFFFFFFULL;
}

inline bool validkernelpointer(std::uintptr_t) noexcept
{
	return false;
}

inline bool invalidptr(std::uintptr_t pointer) noexcept
{
	return !validpointer(pointer);
}

namespace mem_bridge
{
	// Astrum HAL/ring stack (same as pawjob-rust). Keeps the arc-um
	// mem_ctx call surface; CR3 is informational / for existing recover
	// loops — the driver translates VA by PID itself.
	class mem_ctx
	{
	public:
		INT32 process_id = 0;
		std::uintptr_t image_base = 0;
		std::uintptr_t cr3 = 0;

		// 0 = ok (matches main.cpp checks)
		int initialize_handle() noexcept
		{
			if (m_mem.has_ring() || m_mem.is_driver_connected())
				return 0;
			return m_mem.try_connect() ? 0 : 1;
		}

		bool is_ready() const noexcept
		{
			return m_mem.is_driver_connected();
		}

		TransportStatus read_physical(PVOID address, PVOID buffer, DWORD size) const noexcept
		{
			if (!address || !buffer || !size || !process_id)
			{
				zero_buffer(buffer, size);
				LogTransportStatus(TransportStatus::InvalidTarget);
				return TransportStatus::InvalidTarget;
			}
			if (!validpointer(reinterpret_cast<std::uintptr_t>(address)))
			{
				zero_buffer(buffer, size);
				LogTransportStatus(TransportStatus::InvalidTarget);
				return TransportStatus::InvalidTarget;
			}
			auto attachStatus = ensure_attached();
			if (attachStatus != TransportStatus::Ok)
			{
				zero_buffer(buffer, size);
				LogTransportStatus(attachStatus);
				return attachStatus;
			}
			if (!m_mem.read(reinterpret_cast<uint64_t>(address), buffer, size))
			{
				zero_buffer(buffer, size);
				LogTransportStatus(TransportStatus::ReadFailed);
				return TransportStatus::ReadFailed;
			}
			return TransportStatus::Ok;
		}

		TransportStatus write_physical(PVOID address, PVOID buffer, DWORD size) const noexcept
		{
			if (!address || !buffer || !size || !process_id)
			{
				LogTransportStatus(TransportStatus::InvalidTarget);
				return TransportStatus::InvalidTarget;
			}
			if (!validpointer(reinterpret_cast<std::uintptr_t>(address)))
			{
				LogTransportStatus(TransportStatus::InvalidTarget);
				return TransportStatus::InvalidTarget;
			}
			auto attachStatus = ensure_attached();
			if (attachStatus != TransportStatus::Ok)
			{
				LogTransportStatus(attachStatus);
				return attachStatus;
			}
			if (!m_mem.write(buffer, reinterpret_cast<uint64_t>(address), size))
			{
				LogTransportStatus(TransportStatus::WriteFailed);
				return TransportStatus::WriteFailed;
			}
			return TransportStatus::Ok;
		}

		std::uintptr_t resolve_cr3_aggressive(
			const std::uint32_t& pid,
			std::uintptr_t base = 0) const noexcept
		{
			if (!pid)
				return 0;
			if (!m_mem.is_driver_connected() && !m_mem.try_connect())
				return 0;
			if (!m_mem.attach(pid))
				return 0;

			const std::uintptr_t dtb =
				static_cast<std::uintptr_t>(m_mem.get_dtb());
			if (!dtb)
				return 0;

			const std::uintptr_t check_base =
				base ? base
					 : static_cast<std::uintptr_t>(
						   reinterpret_cast<uintptr_t>(m_mem.get_base_address()));
			if (check_base)
			{
				std::uint16_t mz = 0;
				if (!m_mem.read(check_base, &mz, sizeof(mz)) || mz != 0x5A4D)
					return 0;
			}

			return dtb;
		}

		std::uintptr_t get_cr3(const std::uint32_t& pid) const noexcept
		{
			return resolve_cr3_aggressive(pid, image_base);
		}

		std::uintptr_t get_module_base(const wchar_t* module_name) const noexcept
		{
			if (!module_name || !process_id || ensure_attached() != TransportStatus::Ok)
				return 0;
			return static_cast<std::uintptr_t>(
				m_mem.get_module_base(module_name));
		}

		std::uintptr_t find_image() const noexcept
		{
			if (!process_id || ensure_attached() != TransportStatus::Ok)
				return 0;

			static const wchar_t* kNames[] = {
				L"PioneerGame.exe",
				L"PioneerGame-d.exe",
				L"PioneerGame-e.exe",
				L"PioneerGame-Win64-Shipping.exe",
			};
			for (auto* n : kNames)
			{
				const auto base = static_cast<std::uintptr_t>(
					m_mem.get_module_base(n));
				if (base)
					return base;
			}

			return static_cast<std::uintptr_t>(
				reinterpret_cast<uintptr_t>(m_mem.get_base_address()));
		}

		INT32 find_process(LPCTSTR process_name) noexcept
		{
			process_id = 0;
			if (!process_name)
				return 0;

			PROCESSENTRY32W pe{};
			pe.dwSize = sizeof(pe);
			HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (snap == INVALID_HANDLE_VALUE)
				return 0;

#ifdef UNICODE
			const wchar_t* target = process_name;
#else
			wchar_t wide[MAX_PATH]{};
			MultiByteToWideChar(CP_ACP, 0, process_name, -1, wide, MAX_PATH);
			const wchar_t* target = wide;
#endif

			if (Process32FirstW(snap, &pe))
			{
				do
				{
					if (_wcsicmp(pe.szExeFile, target) == 0)
					{
						process_id = static_cast<INT32>(pe.th32ProcessID);
						CloseHandle(snap);
						return process_id;
					}
				} while (Process32NextW(snap, &pe));
			}

			CloseHandle(snap);
			return 0;
		}

		template <typename T>
		std::pair<T, TransportStatus> read_status(std::uint64_t address) const noexcept
		{
			T buffer{};
			auto status = read_physical(reinterpret_cast<PVOID>(address), &buffer, sizeof(T));
			return { buffer, status };
		}

		template <typename T>
		T read(std::uint64_t address) const noexcept
		{
			auto [value, status] = read_status<T>(address);
			m_last_status = status;
			return value;
		}

		template <typename T>
		T read_stale(std::uint64_t address, bool& stale) const noexcept
		{
			auto [value, status] = read_status<T>(address);
			m_last_status = status;
			if (status != TransportStatus::Ok)
				stale = true;
			return value;
		}

		template <typename T>
		TransportStatus write_status(std::uint64_t address, T value) const noexcept
		{
			return write_physical(reinterpret_cast<PVOID>(address), &value, sizeof(T));
		}

		template <typename T>
		T write(std::uint64_t address, T value) const noexcept
		{
			m_last_status = write_status(address, value);
			return value;
		}

		template <typename Type>
		Type ReadChain(
			const std::uint64_t& base,
			std::vector<std::uint64_t> offsets) const noexcept
		{
			std::uint64_t cur = base;
			if (offsets.empty())
				return read<Type>(cur);

			for (std::size_t i = 0; i + 1 < offsets.size(); ++i)
			{
				cur = read<std::uint64_t>(cur + offsets[i]);
				if (!validpointer(cur))
					return Type{};
			}
			return read<Type>(cur + offsets.back());
		}

		std::wstring ReadWString(std::uint64_t address, int max_chars) const
		{
			if (!address || max_chars <= 0)
				return {};
			std::wstring out(static_cast<std::size_t>(max_chars), L'\0');
			m_last_status = read_physical(
					reinterpret_cast<PVOID>(address),
					out.data(),
					static_cast<DWORD>(max_chars * sizeof(wchar_t)));
			if (m_last_status != TransportStatus::Ok)
				return {};
			const auto n = out.find(L'\0');
			if (n != std::wstring::npos)
				out.resize(n);
			return out;
		}

		bool read_array2(void* buffer, std::uintptr_t address, std::size_t size) const noexcept
		{
			m_last_status = read_physical(
				reinterpret_cast<PVOID>(address),
				buffer,
				static_cast<DWORD>(size));
			return m_last_status == TransportStatus::Ok;
		}

		bool read_array(std::uint64_t address, void* buffer, std::size_t size) const noexcept
		{
			m_last_status = read_physical(
				reinterpret_cast<PVOID>(address),
				buffer,
				static_cast<DWORD>(size));
			return m_last_status == TransportStatus::Ok;
		}

		std::string read_string(std::uintptr_t address) const
		{
			char buf[256]{};
			m_last_status = read_physical(reinterpret_cast<PVOID>(address), buf, sizeof(buf) - 1);
			if (m_last_status != TransportStatus::Ok)
				return {};
			return std::string(buf);
		}

		std::uint64_t get_kernel_cr3() const noexcept
		{
			return 0;
		}

		c_memory& mem() noexcept { return m_mem; }

		TransportStatus last_status() const noexcept { return m_last_status; }
		bool transport_lost() const noexcept { return m_mem.transport_lost(); }
		bool session_ended() const noexcept { return m_mem.session_ended(); }

	private:
		mutable c_memory m_mem;
		mutable TransportStatus m_last_status = TransportStatus::Ok;

		TransportStatus ensure_attached() const noexcept
		{
			if (!process_id)
				return TransportStatus::InvalidTarget;
			if (!m_mem.is_driver_connected() && !m_mem.try_connect())
			{
				if (m_mem.session_ended())
					return TransportStatus::Detached;
				return TransportStatus::NotReady;
			}
			if (m_mem.get_pid() == static_cast<uint32_t>(process_id))
				return TransportStatus::Ok;
			if (!m_mem.attach(static_cast<uint32_t>(process_id)))
				return m_mem.transport_lost() ? TransportStatus::Detached : TransportStatus::NotReady;
			return TransportStatus::Ok;
		}

		static void zero_buffer(void* buffer, std::size_t size) noexcept
		{
			if (buffer != nullptr && size != 0)
				std::memset(buffer, 0, size);
		}
	};

	inline mem_ctx context{};
}

inline mem_bridge::mem_ctx* ctx = &mem_bridge::context;
