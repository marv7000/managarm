#include <eir-internal/arch.hpp>
#include <eir-internal/generic.hpp>
#include <efi/protocol/loaded-image.h>
#include <efi.hpp>
#include <stdbool.h>
#include <eir-internal/debug.hpp>

#define ERR(x) if(EFI_ERROR((x))) return (x)

// TODO: REMOVE ME
#define BLOCK while((status = st->ConIn->ReadKeyStroke(st->ConIn, &inputKey)) == EFI_NOT_READY);

static_assert(sizeof(char16_t) == sizeof(wchar_t), "Strings are not UTF-16, missing -fshort-wchar?");

efi_system_table *systemTable = NULL;

namespace eir {

extern "C" efi_status efi_main(efi_handle handle, efi_system_table *st) {
	efi_status status;
	efi_input_key inputKey;

	// Set the system table so we can use loggers early on.
	systemTable = st;

	// Reset the watchdog timer.
	status = st->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
	ERR(status);
	status = st->ConOut->ClearScreen(st->ConOut);
	ERR(status);

	// Get a handle to this binary in order to get the command line.
	const efi_guid protocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	efi_loaded_image_protocol *loadedImage = nullptr;
	status = st->BootServices->HandleProtocol(handle, &protocol, reinterpret_cast<void**>(&loadedImage));
	ERR(status);

	// Convert the cmdline from UTF-16 to UTF-8.
	for(size_t i = 0; i < loadedImage->LoadOptionsSize / sizeof(uint16_t); i++) {
		eir::infoLogger() << ((uint16_t*)loadedImage->LoadOptions)[i];
	}

	eir::infoLogger() << frg::endlog;
	BLOCK

	size_t memMapSize, mapKey, descriptorSize = 0;
	uint32_t descriptorVersion = 0;
	void* memMap = nullptr;

	eir::infoLogger() << "Memory map:" << frg::endlog;
	// First get the size of the memory map buffer to allocate.
	status = st->BootServices->GetMemoryMap(&memMapSize, nullptr, &mapKey, &descriptorSize, &descriptorVersion);
	ERR(status);
	status = st->BootServices->AllocatePool(EfiReservedMemoryType, memMapSize, &memMap);
	ERR(status);

	// Now, get the actual memory map.
	mapKey = 0;
	descriptorSize = 0;
	descriptorVersion = 0;

	status = st->BootServices->GetMemoryMap(&memMapSize,
		reinterpret_cast<efi_memory_descriptor*>(memMap), &mapKey, &descriptorSize, &descriptorVersion);
	ERR(status);

	InitialRegion reservedRegions[32];
	size_t nReservedRegions = 0;
	for(size_t i = 0; i <  memMapSize / descriptorSize; i++) {
		auto entry = reinterpret_cast<efi_memory_descriptor*>(uintptr_t(memMap) + (i * descriptorSize));
		eir::infoLogger()
			<< "    Type " << entry->Type << " mapping."
			<< " Base: 0x" << frg::hex_fmt{entry->PhysicalStart}
			<< ", length: 0x" << frg::hex_fmt{entry->NumberOfPages * eir::pageSize}
			<< frg::endlog;
		//if(entry->Type == 1)
		//	createInitialRegions(
		//		{entry->PhysicalStart, entry->NumberOfPages * eir::pageSize},
		//		{reservedRegions, nReservedRegions}
		//	);
	}

	setupRegionStructs();

	uint64_t kernel_entry = 0;
	//initProcessorPaging((void *)kernel_module_start, kernel_entry);

	//auto *info_ptr = generateInfo(cmdline.data());
	//auto modules = bootAlloc<EirModule>(n_modules - 1);

	// TODO: Modeset FB
	//

	// TODO REMOVE ME: wait for user input.
	status = st->ConIn->Reset(st->ConIn, false);
	ERR(status);

	BLOCK
	ERR(status);

	// TODO: Hand-off to thor

	// Exit boot services.
	st->BootServices->ExitBootServices(handle, mapKey);

	return EFI_SUCCESS;
}

}
