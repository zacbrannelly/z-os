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

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *simple_file_system_protocol;
EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics_output_protocol;

typedef struct boot_info {
  uint32_t *framebuffer;
  uint32_t framebuffer_size;
  uint32_t framebuffer_width;
  uint32_t framebuffer_stride;
} boot_info_t;

EFI_STATUS exit_boot_services(IN EFI_HANDLE image_handle, IN EFI_SYSTEM_TABLE *system_table) {
  UINTN memory_map_size = 0;
  UINTN map_key = 0;
  UINTN descriptor_size = 0;
  UINT32 descriptor_version = 0;
  EFI_MEMORY_DESCRIPTOR *memory_map = NULL;

  EFI_STATUS status = system_table->BootServices->GetMemoryMap(
    &memory_map_size,
    memory_map,
    &map_key,
    &descriptor_size,
    &descriptor_version
  );

  if (status != EFI_BUFFER_TOO_SMALL) {
    Print(L"GetMemoryMap size query failed: %r\r\n", status);
    return status;
  }

  memory_map_size += 2 * descriptor_size;
  status = system_table->BootServices->AllocatePool(
    EfiLoaderData,
    memory_map_size,
    (VOID **)&memory_map
  );

  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate memory map buffer: %r\r\n", status);
    return status;
  }

  while (TRUE) {
    status = system_table->BootServices->GetMemoryMap(
      &memory_map_size,
      memory_map,
      &map_key,
      &descriptor_size,
      &descriptor_version
    );

    if (EFI_ERROR(status)) {
      Print(L"GetMemoryMap failed: %r\r\n", status);
      return status;
    }

    status = system_table->BootServices->ExitBootServices(image_handle, map_key);
    if (status != EFI_INVALID_PARAMETER) {
      break;
    }
  }

  if (EFI_ERROR(status)) {
    return status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain (
  IN EFI_HANDLE image_handle,
  IN EFI_SYSTEM_TABLE *system_table
) {
  // ================================ Load the kernel ELF file ========================================

  EFI_STATUS status = system_table->BootServices->LocateProtocol(
    &gEfiSimpleFileSystemProtocolGuid,
    NULL,
    (VOID **)&simple_file_system_protocol
  );

  if (EFI_ERROR(status)) {
    Print(L"Failed to locate simple file system protocol: %r\r\n", status);
    return status;
  }

  EFI_FILE_PROTOCOL *file_protocol;
  status = simple_file_system_protocol->OpenVolume(simple_file_system_protocol, &file_protocol);
  if (EFI_ERROR(status)) {
    Print(L"Failed to open volume: %r\r\n", status);
    return status;
  }

  status = file_protocol->Open(file_protocol, &file_protocol, L"\\kernel.elf", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status)) {
    Print(L"Failed to open file: %r\r\n", status);
    return status;
  }

  UINTN file_info_size = 0;
  status = file_protocol->GetInfo(file_protocol, &gEfiFileInfoGuid, &file_info_size, NULL);
  if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
    Print(L"Failed to get file info: %r\r\n", status);
    return status;
  }

  EFI_FILE_INFO *file_info = NULL;
  status = system_table->BootServices->AllocatePool(
    EfiLoaderData,
    file_info_size,
    (VOID **)&file_info
  );
  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate pool for file info: %r\r\n", status);
    return status;
  }

  status = file_protocol->GetInfo(file_protocol, &gEfiFileInfoGuid, &file_info_size, file_info);
  if (EFI_ERROR(status)) {
    Print(L"Failed to get file info: %r\r\n", status);
    return status;
  }

  // Allocate a buffer of the size of the kernel file.
  UINTN kernel_file_size = (UINTN)file_info->FileSize;
  uint8_t *kernel_elf_buffer = NULL;
  status = system_table->BootServices->AllocatePool(
    EfiLoaderData,
    kernel_file_size,
    (VOID **)&kernel_elf_buffer
  );
  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate pool for kernel buffer: %r\r\n", status);
    return status;
  }

  status = file_protocol->Read(file_protocol, &kernel_file_size, kernel_elf_buffer);
  if (EFI_ERROR(status)) {
    Print(L"Failed to read file: %r\r\n", status);
    return status;
  }

  kernel_elf_info_t kernel_elf_info;
  if (kernel_elf_parse_info(kernel_elf_buffer, &kernel_elf_info) < 0) {
    Print(L"Failed to parse kernel ELF file: %r\r\n", status);
    return status;
  }

  Print(L"Kernel ELF Info:\r\n");
  Print(L"Entry Point: 0x%lx\r\n", kernel_elf_info.entry_point);
  Print(L"Loadable Segment Start: 0x%lx\r\n", kernel_elf_info.segments[0].loadable_segment_start);
  Print(L"Loadable Segment Memory Size: 0x%lx\r\n", kernel_elf_info.segments[0].loadable_segment_memory_size);
  Print(L"Loadable Segment File Size: 0x%lx\r\n", kernel_elf_info.segments[0].loadable_segment_file_size);
  Print(L"Image Start: 0x%lx\r\n", kernel_elf_info.segments[0].image_start);
  Print(L"Program Alignment: 0x%lx\r\n", kernel_elf_info.segments[0].alignment);
  Print(L"Program Flags: 0x%lx\r\n", kernel_elf_info.segments[0].flags);

  // ============================================ Setup the translation table ============================================

  // Create VA table.
  virtual_addr_table_t table;
  virtual_addr_allocate_table(system_table, &table);

  // Load the kernel into memory and map into VA space.
  kernel_loader_load(system_table, &table, &kernel_elf_info);

  // ============================================ Setup the boot info ============================================

  status = system_table->BootServices->LocateProtocol(
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    (VOID **)&graphics_output_protocol
  );

  if (EFI_ERROR(status) || graphics_output_protocol == NULL) {
    Print(L"Failed to locate graphics output protocol: %r\r\n", status);
    return status;
  }

  boot_info_t boot_info;
  boot_info.framebuffer = (uint32_t *)graphics_output_protocol->Mode->FrameBufferBase;
  boot_info.framebuffer_size = graphics_output_protocol->Mode->Info->HorizontalResolution * graphics_output_protocol->Mode->Info->VerticalResolution;
  boot_info.framebuffer_width = graphics_output_protocol->Mode->Info->HorizontalResolution;
  boot_info.framebuffer_stride = graphics_output_protocol->Mode->Info->HorizontalResolution * sizeof(uint32_t);

  VOID *boot_info_physical_address = NULL;
  status = system_table->BootServices->AllocatePool(
    EfiLoaderData,
    sizeof(boot_info_t),
    &boot_info_physical_address
  );
  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate pool for boot info: %r\r\n", status);
    return status;
  }

  system_table->BootServices->CopyMem(
    (VOID *)boot_info_physical_address,
    &boot_info,
    sizeof(boot_info_t)
  );

  // ============================================ Clean up memory ============================================

  // Free the file info buffer.
  system_table->BootServices->FreePool(file_info);
  file_info = NULL;

  // Free the kernel buffer.
  system_table->BootServices->FreePool(kernel_elf_buffer);
  kernel_elf_buffer = NULL;

  // ============================================ Exit boot services ============================================
  exit_boot_services(image_handle, system_table);

  // NOTE: NO MORE BOOT SERVICES CAN BE CALLED AFTER THIS POINT.

  // ============================================ Jump to the kernel ============================================

  // Enable MMU for TTBR1, using the VA table we've built.
  virtual_addr_apply_to_ttbr1(&table);

  // TODO: Change the stack pointer to the top of the kernel stack (the end of the mapped stack space).

  // Jump to the kernel's entry point.
  void (*kernel_entry)(boot_info_t *) = (void (*)(boot_info_t *))kernel_elf_info.entry_point;
  kernel_entry((boot_info_t *)boot_info_physical_address);

  return EFI_SUCCESS;
}
