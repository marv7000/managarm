#include <eir-internal/debug.hpp>
#include <eir-internal/generic.hpp>
#include <eir-internal/arch.hpp>
#include <frg/logging.hpp>
#include <efi.hpp>

extern efi_system_table *systemTable;

namespace eir {

constinit OutputSink infoSink;
constinit frg::stack_buffer_logger<LogSink> infoLogger;
constinit frg::stack_buffer_logger<PanicSink> panicLogger;

void OutputSink::print(char c) {
	if(systemTable) {
		if(c == '\n') {
			systemTable->ConOut->OutputString(systemTable->ConOut, (char16_t*)L"\r\n");
			return;
		}
		char16_t converted[2] = {(char16_t)c, 0};
		systemTable->ConOut->OutputString(systemTable->ConOut, converted);
	}
}

void OutputSink::print(const char *str) {
	while(*str)
		print(*(str++));
}

void LogSink::operator()(const char *c) {
	infoSink.print(c);
	infoSink.print('\n');
}

void PanicSink::operator()(const char *c) {
	infoSink.print(c);
	infoSink.print('\n');
	while(true)
		asm volatile("" : : : "memory");
}

} // namespace eir

extern "C" void __assert_fail(const char *assertion, const char *file,
		unsigned int line, const char *function) {
	eir::panicLogger() << "Assertion failed: " << assertion << "\n"
			<< "In function " << function
			<< " at " << file << ":" << line << frg::endlog;
}

extern "C" void __cxa_pure_virtual() {
	eir::panicLogger() << "Pure virtual call" << frg::endlog;
}

extern "C" void FRG_INTF(panic)(const char *cstring) {
	eir::panicLogger() << "frg: Panic! " << cstring << frg::endlog;
}
