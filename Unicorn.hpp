#pragma once
#include "memory/interface/ctx.hpp"
#include "dgb/Spoof.h"
#include <vector>
#include <mutex>
#include <emmintrin.h>
#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <unicorn/unicorn.h>
#include <unordered_set>
#include <cstring>

constexpr size_t PAGE_SIZE = 0x1000;
constexpr uint64_t STACK_BASE = 0x500000000;
constexpr uint64_t STACK_SIZE = 0x20000;
constexpr uint64_t AUX_BASE = 0x600000000;
constexpr uint64_t AUX_SIZE = PAGE_SIZE * 64;

inline uint64_t AlignPage(uint64_t addr) {
	return addr & ~(PAGE_SIZE - 1);
}

inline void EnsureUnicornTcgBuffer() {
	static volatile LONG initialized = 0;
	static void* reserved1 = nullptr;
	static void* reserved2 = nullptr;
	static void* reserved3 = nullptr;

	if (!InterlockedExchange(&initialized, 1)) {
		reserved1 = VirtualAlloc(reinterpret_cast<void*>(0x7FFF0000),
			PAGE_SIZE * 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

		reserved2 = VirtualAlloc(reinterpret_cast<void*>(0x80000000),
			PAGE_SIZE * 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

		reserved3 = VirtualAlloc(reinterpret_cast<void*>(0x80010000),
			PAGE_SIZE * 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

		if (!reserved1 && !reserved2 && !reserved3) {
			VirtualAlloc(nullptr, PAGE_SIZE * 128,
				MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		}
	}
}

inline uint64_t GetTargetProcessTEB() {
	static uint64_t cached_teb = 0;
	static bool initialized = false;

	if (initialized && cached_teb) return cached_teb;

	typedef struct _THREAD_BASIC_INFO {
		NTSTATUS ExitStatus;
		PVOID TebBaseAddress;
		struct { HANDLE UniqueProcess; HANDLE UniqueThread; } ClientId;
		ULONG_PTR AffinityMask;
		LONG Priority;
		LONG BasePriority;
	} THREAD_BASIC_INFO;

	typedef NTSTATUS(NTAPI* pNtQueryInformationThread)(HANDLE, ULONG, PVOID, ULONG, PULONG);
	static auto NtQueryInformationThread = (pNtQueryInformationThread)GetProcAddress(
		GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");

	if (!NtQueryInformationThread || !ctx) return 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

	THREADENTRY32 te = { sizeof(THREADENTRY32) };
	if (Thread32First(hSnapshot, &te)) {
		do {
			if (te.th32OwnerProcessID == ctx->process_id) {
				HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION |
					THREAD_QUERY_LIMITED_INFORMATION,
					FALSE, te.th32ThreadID);
				if (hThread) {
					THREAD_BASIC_INFO tbi = { 0 };
					if (NT_SUCCESS(NtQueryInformationThread(hThread, 0, &tbi,
						sizeof(tbi), nullptr)) &&
						tbi.TebBaseAddress) {
						cached_teb = (uint64_t)tbi.TebBaseAddress;
						CloseHandle(hThread);
						break;
					}
					CloseHandle(hThread);
				}
			}
		} while (Thread32Next(hSnapshot, &te));
	}
	CloseHandle(hSnapshot);

	initialized = true;
	return cached_teb;
}

class VTableEmulator {
private:
	uc_engine* m_uc = nullptr;
	uc_hook m_hook_mem = 0;
	uc_hook m_hook_code = 0;

	uint64_t m_result = 0;
	uint32_t m_instruction_count = 0;
	uint64_t m_teb_addr = 0;
	uint64_t m_original_rsp = 0;
	bool m_track_rsp = false;
	uint64_t m_intercept_addr = 0;

	std::unordered_set<uint64_t> m_mapped_pages;
	std::mutex m_mutex;

	static VTableEmulator* s_instance;

	static bool HandleUnmappedMemoryStatic(uc_engine* uc, uc_mem_type type,
		uint64_t address, int size,
		int64_t value, void* user_data) {
		(void)type; (void)size; (void)value; (void)user_data;
		if (s_instance) {
			uint64_t base = AlignPage(address);
			if (s_instance->m_mapped_pages.count(base)) return true;

			if (s_instance->m_mapped_pages.size() >= 7000) {
				uc_emu_stop(uc);
				return false;
			}

			std::vector<uint8_t> buf(PAGE_SIZE);
			if (ctx && ctx->read_array(base, buf.data(), PAGE_SIZE)) {
				if (uc_mem_map(uc, base, PAGE_SIZE, UC_PROT_ALL) == UC_ERR_OK) {
					uc_mem_write(uc, base, buf.data(), PAGE_SIZE);
					s_instance->m_mapped_pages.insert(base);
					return true;
				}
			}
			else {
				if (uc_mem_map(uc, base, PAGE_SIZE, UC_PROT_ALL) == UC_ERR_OK) {
					std::vector<uint8_t> zeros(PAGE_SIZE, 0);
					uc_mem_write(uc, base, zeros.data(), PAGE_SIZE);
					s_instance->m_mapped_pages.insert(base);
					return true;
				}
			}
		}
		return false;
	}

	static void HandleCodeExecutionStatic(uc_engine* uc, uint64_t address,
		uint32_t size, void* user_data) {
		(void)size; (void)user_data;
		if (!s_instance) return;

		s_instance->m_instruction_count++;

		if (s_instance->m_instruction_count > 10000) {
			uc_emu_stop(uc);
			return;
		}

		if (s_instance->m_intercept_addr && address == s_instance->m_intercept_addr) {
			uint64_t rcx = 0;
			uc_reg_read(uc, UC_X86_REG_RCX, &rcx);
			s_instance->m_result = rcx;
			uc_emu_stop(uc);
			return;
		}

		uint8_t code[2] = { 0 };
		uc_mem_read(uc, address, code, 2);

		if (code[0] == 0xC3 || code[0] == 0xC2) {
			if (s_instance->m_track_rsp) {
				uint64_t rsp = 0;
				uc_reg_read(uc, UC_X86_REG_RSP, &rsp);
				if (rsp < s_instance->m_original_rsp)
					return;
			}

			uint8_t xmm0_bytes[16] = { 0 };
			uc_reg_read(uc, UC_X86_REG_XMM0, xmm0_bytes);
			uint64_t result_xmm = *(uint64_t*)xmm0_bytes;

			if (validpointer(result_xmm)) {
				s_instance->m_result = result_xmm;
			}
			else {
				uint64_t rax = 0;
				uc_reg_read(uc, UC_X86_REG_RAX, &rax);
				if (validpointer(rax)) {
					s_instance->m_result = rax;
				}
			}
			uc_emu_stop(uc);
		}
	}

public:
	VTableEmulator() = default;

	~VTableEmulator() {
		Shutdown();
	}

	void Shutdown() {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_uc) {
			if (m_hook_mem) uc_hook_del(m_uc, m_hook_mem);
			if (m_hook_code) uc_hook_del(m_uc, m_hook_code);
			uc_close(m_uc);
			m_uc = nullptr;
		}
		m_mapped_pages.clear();
		s_instance = nullptr;
	}

	uint64_t Call(uint64_t game_base, uint64_t func_ptr,
		uint64_t rcx_param, uint64_t rdx_param) {
		(void)game_base;
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!func_ptr || !validpointer(func_ptr)) return 0;

		if (m_uc) {
			if (m_hook_mem) uc_hook_del(m_uc, m_hook_mem);
			if (m_hook_code) uc_hook_del(m_uc, m_hook_code);
			uc_close(m_uc);
			m_uc = nullptr;
			m_mapped_pages.clear();
		}

		m_result = 0;
		m_instruction_count = 0;
		m_track_rsp = false;
		m_intercept_addr = 0;

		if (uc_open(UC_ARCH_X86, UC_MODE_64, &m_uc) != UC_ERR_OK) return 0;

		s_instance = this;
		EnsureUnicornTcgBuffer();

		if (uc_mem_map(m_uc, STACK_BASE, STACK_SIZE, UC_PROT_READ | UC_PROT_WRITE) != UC_ERR_OK) {
			uc_close(m_uc);
			m_uc = nullptr;
			return 0;
		}

		std::vector<uint8_t> stack_zero(STACK_SIZE, 0);
		uc_mem_write(m_uc, STACK_BASE, stack_zero.data(), STACK_SIZE);

		uint64_t code_base = AlignPage(func_ptr - 0x10000);
		uint64_t code_size = 0x50000;

		if (uc_mem_map(m_uc, code_base, code_size, UC_PROT_ALL) == UC_ERR_OK) {
			std::vector<uint8_t> page_buf(PAGE_SIZE);
			for (uint64_t page = code_base; page < code_base + code_size; page += PAGE_SIZE) {
				if (ctx && ctx->read_array(page, page_buf.data(), PAGE_SIZE)) {
					uc_mem_write(m_uc, page, page_buf.data(), PAGE_SIZE);
				}
				m_mapped_pages.insert(page);
			}
		}

		if (!m_teb_addr) {
			m_teb_addr = GetTargetProcessTEB();
		}

		if (m_teb_addr && validpointer(m_teb_addr)) {
			uint64_t teb_page = AlignPage(m_teb_addr);
			for (int i = 0; i < 16; i++) {
				uint64_t page = teb_page + i * PAGE_SIZE;
				if (!m_mapped_pages.count(page)) {
					std::vector<uint8_t> teb_buf(PAGE_SIZE);
					if (ctx && ctx->read_array(page, teb_buf.data(), PAGE_SIZE)) {
						if (uc_mem_map(m_uc, page, PAGE_SIZE, UC_PROT_READ | UC_PROT_WRITE) == UC_ERR_OK) {
							uc_mem_write(m_uc, page, teb_buf.data(), PAGE_SIZE);
							m_mapped_pages.insert(page);
						}
					}
				}
			}
			uc_reg_write(m_uc, UC_X86_REG_GS_BASE, &m_teb_addr);
		}

		uc_mem_map(m_uc, AUX_BASE, PAGE_SIZE * 128, UC_PROT_READ | UC_PROT_WRITE);

		uc_hook_add(m_uc, &m_hook_mem, UC_HOOK_MEM_UNMAPPED,
			(void*)HandleUnmappedMemoryStatic, nullptr, 1, 0);
		uc_hook_add(m_uc, &m_hook_code, UC_HOOK_CODE,
			(void*)HandleCodeExecutionStatic, nullptr, 1, 0);

		uint64_t rcx_data_addr = STACK_BASE + 0x400;
		uint64_t rdx_data_addr = STACK_BASE + 0x500;

		if (ctx) {
			__m128i rcx_xmm = ctx->read<__m128i>(rcx_param);
			uc_mem_write(m_uc, rcx_data_addr, &rcx_xmm, sizeof(__m128i));

			__m128i rdx_xmm = ctx->read<__m128i>(rdx_param);
			uc_mem_write(m_uc, rdx_data_addr, &rdx_xmm, sizeof(__m128i));
		}

		uint64_t rsp = STACK_BASE + STACK_SIZE - 0x1000;
		uint64_t ret_addr = 0xDEADC0DEULL;
		uc_mem_write(m_uc, rsp, &ret_addr, 8);

		uc_reg_write(m_uc, UC_X86_REG_RCX, &rcx_data_addr);
		uc_reg_write(m_uc, UC_X86_REG_RDX, &rdx_data_addr);
		uc_reg_write(m_uc, UC_X86_REG_RSP, &rsp);

		uint64_t zero = 0;
		uc_reg_write(m_uc, UC_X86_REG_RAX, &zero);
		uint8_t xmm_zero[16] = { 0 };
		uc_reg_write(m_uc, UC_X86_REG_XMM0, xmm_zero);
		uc_reg_write(m_uc, UC_X86_REG_XMM1, xmm_zero);

		uc_emu_start(m_uc, func_ptr, 0, 500000, 10000);

		uint64_t result = m_result;
		if (result == 0) {
			uint8_t xmm0_bytes[16] = { 0 };
			uc_reg_read(m_uc, UC_X86_REG_XMM0, xmm0_bytes);
			result = *(uint64_t*)xmm0_bytes;

			if (!validpointer(result)) {
				uint64_t rax = 0;
				uc_reg_read(m_uc, UC_X86_REG_RAX, &rax);
				if (validpointer(rax)) {
					result = rax;
				}
			}
		}

		return result;
	}

	uint64_t CallWithR8(uint64_t game_base, uint64_t func_ptr,
		uint64_t rcx_raw, uint64_t r8_param_addr) {
		(void)game_base;
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!func_ptr || !validpointer(func_ptr)) return 0;

		if (m_uc) {
			if (m_hook_mem) uc_hook_del(m_uc, m_hook_mem);
			if (m_hook_code) uc_hook_del(m_uc, m_hook_code);
			uc_close(m_uc);
			m_uc = nullptr;
			m_mapped_pages.clear();
		}

		m_result = 0;
		m_instruction_count = 0;
		m_track_rsp = false;
		m_intercept_addr = 0;

		if (uc_open(UC_ARCH_X86, UC_MODE_64, &m_uc) != UC_ERR_OK) return 0;

		s_instance = this;
		EnsureUnicornTcgBuffer();

		if (uc_mem_map(m_uc, STACK_BASE, STACK_SIZE, UC_PROT_READ | UC_PROT_WRITE) != UC_ERR_OK) {
			uc_close(m_uc);
			m_uc = nullptr;
			return 0;
		}

		std::vector<uint8_t> stack_zero(STACK_SIZE, 0);
		uc_mem_write(m_uc, STACK_BASE, stack_zero.data(), STACK_SIZE);

		uint64_t code_base = AlignPage(func_ptr - 0x10000);
		uint64_t code_size = 0x50000;

		if (uc_mem_map(m_uc, code_base, code_size, UC_PROT_ALL) == UC_ERR_OK) {
			std::vector<uint8_t> page_buf(PAGE_SIZE);
			for (uint64_t page = code_base; page < code_base + code_size; page += PAGE_SIZE) {
				if (ctx && ctx->read_array(page, page_buf.data(), PAGE_SIZE)) {
					uc_mem_write(m_uc, page, page_buf.data(), PAGE_SIZE);
				}
				m_mapped_pages.insert(page);
			}
		}

		if (!m_teb_addr) {
			m_teb_addr = GetTargetProcessTEB();
		}

		if (m_teb_addr && validpointer(m_teb_addr)) {
			uint64_t teb_page = AlignPage(m_teb_addr);
			for (int i = 0; i < 16; i++) {
				uint64_t page = teb_page + i * PAGE_SIZE;
				if (!m_mapped_pages.count(page)) {
					std::vector<uint8_t> teb_buf(PAGE_SIZE);
					if (ctx && ctx->read_array(page, teb_buf.data(), PAGE_SIZE)) {
						if (uc_mem_map(m_uc, page, PAGE_SIZE, UC_PROT_READ | UC_PROT_WRITE) == UC_ERR_OK) {
							uc_mem_write(m_uc, page, teb_buf.data(), PAGE_SIZE);
							m_mapped_pages.insert(page);
						}
					}
				}
			}
			uc_reg_write(m_uc, UC_X86_REG_GS_BASE, &m_teb_addr);
		}

		uc_mem_map(m_uc, AUX_BASE, PAGE_SIZE * 128, UC_PROT_READ | UC_PROT_WRITE);

		uc_hook_add(m_uc, &m_hook_mem, UC_HOOK_MEM_UNMAPPED,
			(void*)HandleUnmappedMemoryStatic, nullptr, 1, 0);
		uc_hook_add(m_uc, &m_hook_code, UC_HOOK_CODE,
			(void*)HandleCodeExecutionStatic, nullptr, 1, 0);

		uint64_t rdx_out_addr = STACK_BASE + 0x600;
		uint64_t r8_data_addr = STACK_BASE + 0x700;

		if (ctx && r8_param_addr) {
			__m128i r8_xmm = ctx->read<__m128i>(r8_param_addr);
			uc_mem_write(m_uc, r8_data_addr, &r8_xmm, sizeof(__m128i));
		}

		uint8_t zero_buf[16] = { 0 };
		uc_mem_write(m_uc, rdx_out_addr, zero_buf, sizeof(zero_buf));

		uint64_t rsp = STACK_BASE + STACK_SIZE - 0x1000;
		uint64_t ret_addr = 0xDEADC0DEULL;
		uc_mem_write(m_uc, rsp, &ret_addr, 8);

		uc_reg_write(m_uc, UC_X86_REG_RCX, &rcx_raw);
		uc_reg_write(m_uc, UC_X86_REG_RDX, &rdx_out_addr);
		uc_reg_write(m_uc, UC_X86_REG_R8, &r8_data_addr);
		uc_reg_write(m_uc, UC_X86_REG_RSP, &rsp);

		uint64_t zero = 0;
		uc_reg_write(m_uc, UC_X86_REG_RAX, &zero);
		uc_reg_write(m_uc, UC_X86_REG_R9, &zero);
		uint8_t xmm_zero[16] = { 0 };
		uc_reg_write(m_uc, UC_X86_REG_XMM0, xmm_zero);
		uc_reg_write(m_uc, UC_X86_REG_XMM1, xmm_zero);

		uc_emu_start(m_uc, func_ptr, 0, 500000, 10000);

		uint8_t out_bytes[16] = { 0 };
		uc_mem_read(m_uc, rdx_out_addr, out_bytes, sizeof(out_bytes));
		uint64_t result = *(uint64_t*)out_bytes;

		if (!validpointer(result)) {
			uint8_t xmm0_bytes[16] = { 0 };
			uc_reg_read(m_uc, UC_X86_REG_XMM0, xmm0_bytes);
			result = *(uint64_t*)xmm0_bytes;

			if (!validpointer(result)) {
				uint64_t rax = 0;
				uc_reg_read(m_uc, UC_X86_REG_RAX, &rax);
				if (validpointer(rax)) {
					result = rax;
				}
			}
		}

		return result;
	}

	static VTableEmulator& Instance() {
		static VTableEmulator instance;
		return instance;
	}
};

inline VTableEmulator* VTableEmulator::s_instance = nullptr;

inline uint64_t EmulateVTableCallWithTLS(uint64_t GameBase, uint64_t funcPtr,
	uint64_t rcx_param, uint64_t rdx_param) {
	return VTableEmulator::Instance().Call(GameBase, funcPtr, rcx_param, rdx_param);
}

inline uint64_t EmulateVTableCallWithTLS_R8(uint64_t GameBase, uint64_t funcPtr,
	uint64_t rcx_raw, uint64_t r8_param_addr) {
	return VTableEmulator::Instance().CallWithR8(GameBase, funcPtr, rcx_raw, r8_param_addr);
}
