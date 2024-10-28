#include <eir-internal/arch.hpp>
#include <eir-internal/generic.hpp>
#include <efi/protocol/loaded-image.h>
#include <efi/protocol/graphics-output.h>
#include <efi.hpp>
#include <stdbool.h>
#include <eir-internal/debug.hpp>
#include <assert.h>

#define EFI_ASSERT(code) { \
	efi_status status = (code); \
	if (status != EFI_SUCCESS) eir::infoLogger() << "EFI ERROR 0x" << frg::hex_fmt {(~EFI_ERROR_MASK & (status))} << frg::endlog; \
	assert(status == EFI_SUCCESS); \
}

static_assert(sizeof(char16_t) == sizeof(wchar_t), "Strings are not UTF-16, missing -fshort-wchar?");
extern "C" void eirEnterKernel(uintptr_t, uint64_t, uint64_t);

namespace eir {

efi_system_table *systemTable = NULL;
efi_handle handle;

efi_status fsOpen(efi_file_protocol **file, char16_t *path) {
	efi_guid loadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	efi_guid simpleFsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

	efi_loaded_image_protocol *loadedImage;
	EFI_ASSERT(systemTable->BootServices->HandleProtocol(handle, &loadedImageGuid, (void **) &loadedImage));

	efi_simple_file_system_protocol *fileSystem;
	EFI_ASSERT(systemTable->BootServices->HandleProtocol(loadedImage->DeviceHandle, &simpleFsGuid, (void **) &fileSystem));

	efi_file_protocol *root;
	EFI_ASSERT(fileSystem->OpenVolume(fileSystem, &root));

	EFI_ASSERT(root->Open(root, file, path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY));

	return EFI_SUCCESS;
}

efi_status fsRead(efi_file_protocol *file, size_t len, size_t offset, void *buf) {
	EFI_ASSERT(file->SetPosition(file, offset));
	EFI_ASSERT(file->Read(file, &len, buf));
	EFI_ASSERT(file->SetPosition(file, 0));

	return EFI_SUCCESS;
}

efi_file_info *fsGetInfo(efi_file_protocol *file) {
	efi_file_info *fileInfo = NULL;
	efi_guid guid = EFI_FILE_INFO_GUID;
	uintptr_t infoLen = 0x1;

	efi_status stat = file->GetInfo(file, &guid, &infoLen, fileInfo);
	assert(stat == EFI_SUCCESS || stat == EFI_BUFFER_TOO_SMALL);
	EFI_ASSERT(systemTable->BootServices->AllocatePool(EfiLoaderData, infoLen, reinterpret_cast<void**>(&fileInfo)));
	EFI_ASSERT(file->GetInfo(file, &guid, &infoLen, fileInfo));

	return fileInfo;
}

extern "C" efi_status eirUefiMain(efi_handle h, efi_system_table *st) {
	// Set the system table so we can use loggers early on.
	systemTable = st;
	handle = h;

	// Reset the watchdog timer.
	EFI_ASSERT(st->BootServices->SetWatchdogTimer(0, 0, 0, NULL));
	EFI_ASSERT(st->ConOut->ClearScreen(st->ConOut));

	// Get a handle to this binary in order to get the command line.
	const efi_guid protocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	efi_loaded_image_protocol *loadedImage = nullptr;
	EFI_ASSERT(st->BootServices->HandleProtocol(handle, &protocol, reinterpret_cast<void**>(&loadedImage)));

	// Convert the command line to ASCII.
	char* cmdLine = nullptr;
	{
		EFI_ASSERT(st->BootServices->AllocatePool(EfiLoaderData,
			(loadedImage->LoadOptionsSize / sizeof(uint16_t)) + 1,
			reinterpret_cast<void**>(&cmdLine)));
		size_t i = 0;
		for(; i < loadedImage->LoadOptionsSize / sizeof(char16_t); i++) {
			char16_t c = ((char16_t*)loadedImage->LoadOptions)[i];
			if(c >= 0x20 && c <= 0x7E) {
				cmdLine[i] = c;
			} else {
				cmdLine[i] = '\0';
			}
		}
		// Null-terminate the buffer.
		cmdLine[i] = '\0';
	}
	assert(cmdLine);

	eir::infoLogger() << "Command line: " << cmdLine << frg::endlog;

	// Load the kernel and initrd.
	efi_file_protocol *kernelProt;
	EFI_ASSERT(fsOpen(&kernelProt, (char16_t*)L"managarm\\thor"));
	uint32_t head;
	fsRead(kernelProt, 4, 0, reinterpret_cast<void*>(&head));
	eir::infoLogger() << "Kernel acb " << frg::hex_fmt {head} << frg::endlog;

	efi_file_protocol *initrdProt;
	EFI_ASSERT(fsOpen(&initrdProt, (char16_t*)L"managarm\\initrd.cpio"));
	efi_file_info *initrdInfo = fsGetInfo(initrdProt);

	// Read initrd.
	void *initrd = nullptr;
	EFI_ASSERT(st->BootServices->AllocatePool(EfiLoaderData, initrdInfo->FileSize, &initrd));
	EFI_ASSERT(fsRead(initrdProt, initrdInfo->FileSize, 0, initrd));

	// Get the frame buffer.
	efi_graphics_output_protocol *gop = nullptr;
	{
		const efi_guid gop_protocol = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
		EFI_ASSERT(st->BootServices->LocateProtocol(&gop_protocol, nullptr, reinterpret_cast<void**>(&gop)));

		eir::infoLogger() << "Framebuffer ("
			<< gop->Mode->Info->HorizontalResolution << "x"
			<< gop->Mode->Info->VerticalResolution << ") Address: 0x"
			<< frg::hex_fmt{gop->Mode->FrameBufferBase}
			<< frg::endlog;
	}

	size_t memMapSize = sizeof(efi_memory_descriptor);
	size_t mapKey, descriptorSize = 0;
	uint32_t descriptorVersion = 0;
	void* memMap = nullptr;
	efi_memory_descriptor dummy;

	// First get the size of the memory map buffer to allocate.
	efi_status status = st->BootServices->GetMemoryMap(&memMapSize, &dummy, &mapKey, &descriptorSize, &descriptorVersion);
	assert(status == EFI_BUFFER_TOO_SMALL); // :^)
	EFI_ASSERT(st->BootServices->AllocatePool(EfiLoaderData, memMapSize, &memMap));
	eir::infoLogger() << "Memory map addr: " << uintptr_t(memMap) << frg::endlog;
	eir::infoLogger() << "Memory map size: " << memMapSize / descriptorSize << " Size in bytes: " << memMapSize << frg::endlog;

	// Now, get the actual memory map.
	EFI_ASSERT(st->BootServices->GetMemoryMap(&memMapSize,
		reinterpret_cast<efi_memory_descriptor*>(memMap),
		&mapKey, &descriptorSize, &descriptorVersion));

	InitialRegion reservedRegions[32];
	size_t nReservedRegions = 0;
	eir::infoLogger() << "Memory map:" << frg::endlog;

	// Iterate over all memory descriptors.
	for(size_t i = 0; i < memMapSize / descriptorSize; i++) {
		auto entry = reinterpret_cast<efi_memory_descriptor*>(uintptr_t(memMap) + (i * descriptorSize));

		eir::infoLogger()
			<< "    Type " << entry->Type << " mapping."
			<< " Base: 0x" << frg::hex_fmt{entry->PhysicalStart}
			<< ", length: 0x" << frg::hex_fmt{entry->NumberOfPages * eir::pageSize}
			<< frg::endlog;

		// Find the first entry we can use to create the initial regions in.
		if(entry->Type == EfiReservedMemoryType) {
			createInitialRegions(
				{entry->PhysicalStart, entry->NumberOfPages * eir::pageSize},
				{reservedRegions, nReservedRegions}
			);
		}
	}
	uint64_t kernel_entry = 0;

	// Exit boot services.
	st->BootServices->ExitBootServices(handle, mapKey);

	initProcessorEarly();
	setupRegionStructs();
	initProcessorPaging((void *)0, kernel_entry);

	EirInfo *info_ptr = generateInfo(cmdLine);

	if(gop) {
		auto fb = &info_ptr->frameBuffer;
		fb->fbAddress = gop->Mode->FrameBufferBase;
		fb->fbPitch = gop->Mode->Info->PixelsPerScanLine;
		fb->fbWidth = gop->Mode->Info->HorizontalResolution;
		fb->fbHeight = gop->Mode->Info->VerticalResolution;

		// TODO: Count bits per pixel.
		fb->fbBpp = gop->Mode->Info->PixelInformation.BlueMask;
		fb->fbType = 0; // TODO: idk what this does ?

		// Map the framebuffer.
		assert(fb->fbAddress & ~static_cast<EirPtr>(pageSize - 1));
		for(address_t pg = 0; pg < gop->Mode->FrameBufferSize; pg += 0x1000)
			mapSingle4kPage(0xFFFF'FE00'4000'0000 + pg, fb->fbAddress + pg,
					PageFlags::write, CachingMode::writeCombine);
		mapKasanShadow(0xFFFF'FE00'4000'0000, fb->fbPitch * fb->fbHeight);
		unpoisonKasanShadow(0xFFFF'FE00'4000'0000, fb->fbPitch * fb->fbHeight);
		fb->fbEarlyWindow = 0xFFFF'FE00'4000'0000;
	}

	// Hand-off to thor
	eir::infoLogger() << "Leaving Eir and entering the real kernel" << frg::endlog;
	eirEnterKernel(0, kernel_entry, 0xFFFF'FE80'0001'0000); // TODO

	return EFI_SUCCESS;
}

} // namespace eir
