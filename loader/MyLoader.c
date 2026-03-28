#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

#include "kernel_elf.h"
#include "kernel_loader.h"
#include "virtual_addr.h"

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystemProtocol;
EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutputProtocol;

typedef struct boot_info {
  uint32_t *framebuffer;
  uint32_t framebuffer_size;
  uint32_t framebuffer_width;
  uint32_t framebuffer_stride;
} boot_info_t;

EFI_STATUS ExitBootServices(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
  UINTN memoryMapSize = 0;
  UINTN mapKey = 0;
  UINTN descriptorSize = 0;
  UINT32 descriptorVersion = 0;
  EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;

  EFI_STATUS Status = SystemTable->BootServices->GetMemoryMap(
    &memoryMapSize,
    memoryMap,
    &mapKey,
    &descriptorSize,
    &descriptorVersion
  );

  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"GetMemoryMap size query failed: %r\r\n", Status);
    return Status;
  }

  memoryMapSize += 2 * descriptorSize;
  Status = SystemTable->BootServices->AllocatePool(
    EfiLoaderData,
    memoryMapSize,
    (VOID **)&memoryMap
  );

  if (EFI_ERROR(Status)) {
    Print(L"Failed to allocate memory map buffer: %r\r\n", Status);
    return Status;
  }

  while (TRUE) {
    Status = SystemTable->BootServices->GetMemoryMap(
      &memoryMapSize,
      memoryMap,
      &mapKey,
      &descriptorSize,
      &descriptorVersion
    );

    if (EFI_ERROR(Status)) {
      Print(L"GetMemoryMap failed: %r\r\n", Status);
      return Status;
    }

    Status = SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);
    if (Status != EFI_INVALID_PARAMETER) {
      break;
    }
  }

  if (EFI_ERROR(Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
) {
  // ================================ Load the kernel ELF file ========================================

  EFI_STATUS Status = SystemTable->BootServices->LocateProtocol(
    &gEfiSimpleFileSystemProtocolGuid,
    NULL,
    (VOID **)&SimpleFileSystemProtocol
  );

  if (EFI_ERROR(Status)) {
    Print(L"Failed to locate simple file system protocol: %r\r\n", Status);
    return Status;
  }

  EFI_FILE_PROTOCOL *FileProtocol;
  Status = SimpleFileSystemProtocol->OpenVolume(SimpleFileSystemProtocol, &FileProtocol);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to open volume: %r\r\n", Status);
    return Status;
  }

  Status = FileProtocol->Open(FileProtocol, &FileProtocol, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to open file: %r\r\n", Status);
    return Status;
  }

  UINTN fileInfoSize = 0;
  Status = FileProtocol->GetInfo(FileProtocol, &gEfiFileInfoGuid, &fileInfoSize, NULL);
  if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"Failed to get file info: %r\r\n", Status);
    return Status;
  }

  EFI_FILE_INFO *fileInfo = NULL;
  Status = SystemTable->BootServices->AllocatePool(
    EfiLoaderData,
    fileInfoSize,
    (VOID **)&fileInfo
  );
  if (EFI_ERROR(Status)) {
    Print(L"Failed to allocate pool for file info: %r\r\n", Status);
    return Status;
  }

  Status = FileProtocol->GetInfo(FileProtocol, &gEfiFileInfoGuid, &fileInfoSize, fileInfo);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to get file info: %r\r\n", Status);
    return Status;
  }

  // Allocate a buffer of the size of the kernel file.
  UINTN kernelFileSize = (UINTN)fileInfo->FileSize;
  uint8_t *kernel_elf_buffer = NULL;
  Status = SystemTable->BootServices->AllocatePool(
    EfiLoaderData,
    kernelFileSize,
    (VOID **)&kernel_elf_buffer
  );
  if (EFI_ERROR(Status)) {
    Print(L"Failed to allocate pool for kernel buffer: %r\r\n", Status);
    return Status;
  }

  Status = FileProtocol->Read(FileProtocol, &kernelFileSize, kernel_elf_buffer);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to read file: %r\r\n", Status);
    return Status;
  }

  kernel_elf_info_t kernelElfInfo;
  if (kernel_elf_parse_info(kernel_elf_buffer, &kernelElfInfo) < 0) {
    Print(L"Failed to parse kernel ELF file: %r\r\n", Status);
    return Status;
  }

  Print(L"Kernel ELF Info:\r\n");
  Print(L"Entry Point: 0x%lx\r\n", kernelElfInfo.entry_point);
  Print(L"Loadable Segment Start: 0x%lx\r\n", kernelElfInfo.segments[0].loadable_segment_start);
  Print(L"Loadable Segment Memory Size: 0x%lx\r\n", kernelElfInfo.segments[0].loadable_segment_memory_size);
  Print(L"Loadable Segment File Size: 0x%lx\r\n", kernelElfInfo.segments[0].loadable_segment_file_size);
  Print(L"Image Start: 0x%lx\r\n", kernelElfInfo.segments[0].image_start);
  Print(L"Program Alignment: 0x%lx\r\n", kernelElfInfo.segments[0].alignment);
  Print(L"Program Flags: 0x%lx\r\n", kernelElfInfo.segments[0].flags);

  // ============================================ Setup the translation table ============================================

  // Create VA table.
  virtual_addr_table_t table;
  virtual_addr_allocate_table(SystemTable, &table);

  // Load the kernel into memory and map into VA space.
  kernel_loader_load(SystemTable, &table, &kernelElfInfo);

  // ============================================ Setup the boot info ============================================

  EFI_STATUS status = SystemTable->BootServices->LocateProtocol(
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    (VOID **)&GraphicsOutputProtocol
  );

  if (EFI_ERROR(status) || GraphicsOutputProtocol == NULL) {
    Print(L"Failed to locate graphics output protocol: %r\r\n", status);
    return status;
  }

  boot_info_t bootInfo;
  bootInfo.framebuffer = (uint32_t *)GraphicsOutputProtocol->Mode->FrameBufferBase;
  bootInfo.framebuffer_size = GraphicsOutputProtocol->Mode->Info->HorizontalResolution * GraphicsOutputProtocol->Mode->Info->VerticalResolution;
  bootInfo.framebuffer_width = GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
  bootInfo.framebuffer_stride = GraphicsOutputProtocol->Mode->Info->HorizontalResolution * sizeof(uint32_t);

  VOID *bootInfoPhysicalAddress = NULL;
  status = SystemTable->BootServices->AllocatePool(
    EfiLoaderData,
    sizeof(boot_info_t),
    &bootInfoPhysicalAddress
  );
  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate pool for boot info: %r\r\n", status);
    return status;
  }

  SystemTable->BootServices->CopyMem(
    (VOID *)bootInfoPhysicalAddress,
    &bootInfo,
    sizeof(boot_info_t)
  );

  // ============================================ Clean up memory ============================================

  // Free the file info buffer.
  SystemTable->BootServices->FreePool(fileInfo);
  fileInfo = NULL;

  // Free the kernel buffer.
  SystemTable->BootServices->FreePool(kernel_elf_buffer);
  kernel_elf_buffer = NULL;

  // ============================================ Exit boot services ============================================
  ExitBootServices(ImageHandle, SystemTable);

  // NOTE: NO MORE BOOT SERVICES CAN BE CALLED AFTER THIS POINT.

  // ============================================ Jump to the kernel ============================================

  // Enable MMU for TTBR1, using the VA table we've built.
  virtual_addr_apply_to_ttbr1(&table);

  // TODO: Change the stack pointer to the top of the kernel stack (the end of the mapped stack space).

  // Jump to the kernel's entry point.
  void (*kernelEntry)(boot_info_t *) = (void (*)(boot_info_t *))kernelElfInfo.entry_point;
  kernelEntry((boot_info_t *)bootInfoPhysicalAddress);

  return EFI_SUCCESS;
}
