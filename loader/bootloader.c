#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

#include "../boot_info.h"
#include "kernel_elf.h"
#include "kernel_loader.h"
#include "virtual_addr.h"

#define KERNEL_STACK_VIRTUAL_BASE 0xffffffffff000000ULL
#define KERNEL_STACK_REGION_SIZE (16 * 1024 * 1024)
#define KERNEL_STACK_REGION_PAGES (KERNEL_STACK_REGION_SIZE / EFI_PAGE_SIZE)

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *simple_file_system_protocol;
EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics_output_protocol;

// Handoff function to jump to the kernel.
void __attribute__((noreturn)) handoff(
  uint64_t kernel_stack_top,
  uint64_t kernel_entry_point,
  boot_info_t *boot_info
);

EFI_STATUS exit_boot_services(IN EFI_HANDLE image_handle, IN EFI_SYSTEM_TABLE *system_table, boot_info_t *boot_info) {
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
    Print(L"Failed to exit boot services: %r\r\n", status);
    return status;
  }

  // Provide the memory map to the kernel.
  boot_info->memory_map = memory_map;
  boot_info->memory_map_size = memory_map_size;
  boot_info->memory_map_descriptor_size = descriptor_size;

  return EFI_SUCCESS;
}

static EFI_STATUS load_file(
  EFI_SYSTEM_TABLE *system_table,
  EFI_FILE_PROTOCOL *file_protocol,
  CHAR16 *file_path,
  uint8_t **file_buffer_ptr,
  UINTN *file_size
) {
  EFI_STATUS status = file_protocol->Open(file_protocol, &file_protocol, file_path, EFI_FILE_MODE_READ, 0);
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

  // Allocate a buffer of the size of the file.
  *file_size = (UINTN)file_info->FileSize;
  status = system_table->BootServices->AllocatePool(
    EfiLoaderData,
    *file_size,
    (VOID **)file_buffer_ptr
  );
  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate file buffer: %r\r\n", status);
    return status;
  }

  status = file_protocol->Read(file_protocol, file_size, *file_buffer_ptr);
  if (EFI_ERROR(status)) {
    Print(L"Failed to read file: %r\r\n", status);
    return status;
  }

  // Clean up
  system_table->BootServices->FreePool(file_info);
  file_info = NULL;

  return EFI_SUCCESS;
}

typedef struct boot_module_info_t {
  char* name;
  CHAR16* path;
} boot_module_info_t;

static const boot_module_info_t g_boot_modules_to_load[] = {
  { "hello_world", L"hello_world.elf" },
  { "compositor", L"compositor.elf" }
};

static const boot_module_info_t g_boot_modules_to_load_tests[] = {
  { "tests", L"tests.elf" }
};

static EFI_STATUS load_boot_module(
  const boot_module_info_t *boot_module,
  EFI_SYSTEM_TABLE *system_table,
  EFI_FILE_PROTOCOL *file_protocol,
  boot_info_t *boot_info
) {
  uint8_t *boot_module_buffer = NULL;
  UINTN boot_module_size = 0;
  EFI_STATUS status = load_file(system_table, file_protocol, boot_module->path, &boot_module_buffer, &boot_module_size);
  if (EFI_ERROR(status)) {
    Print(L"Failed to load boot module: %r\r\n", status);
    return status;
  }

  AsciiStrCpyS(boot_info->boot_modules[boot_info->num_boot_modules].name, 50, boot_module->name);
  boot_info->boot_modules[boot_info->num_boot_modules].elf_buffer = boot_module_buffer;
  boot_info->boot_modules[boot_info->num_boot_modules].elf_size = boot_module_size;
  boot_info->num_boot_modules++;

  return EFI_SUCCESS;
} 

static EFI_STATUS load_boot_modules(
  EFI_SYSTEM_TABLE *system_table,
  EFI_FILE_PROTOCOL *file_protocol,
  boot_info_t *boot_info
) {
  EFI_STATUS status = EFI_SUCCESS;

#if RUN_TESTS == 1
  int num_boot_modules = sizeof(g_boot_modules_to_load_tests) / sizeof(boot_module_info_t);
  const boot_module_info_t *boot_modules = g_boot_modules_to_load_tests;
#else
  int num_boot_modules = sizeof(g_boot_modules_to_load) / sizeof(boot_module_info_t);
  const boot_module_info_t *boot_modules = g_boot_modules_to_load;
#endif

  for (int i = 0; i < num_boot_modules; i++) {
    const boot_module_info_t *boot_module = &boot_modules[i];
    status = load_boot_module(boot_module, system_table, file_protocol, boot_info);
    if (EFI_ERROR(status)) {
      Print(L"Failed to load boot module: %r\r\n", status);
      return status;
    }
  }

  return status;
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

  uint8_t *kernel_elf_buffer = NULL;
  UINTN kernel_elf_size = 0;
  status = load_file(system_table, file_protocol, L"kernel.elf", &kernel_elf_buffer, &kernel_elf_size);
  if (EFI_ERROR(status)) {
    Print(L"Failed to load kernel ELF file: %r\r\n", status);
    return status;
  }

  kernel_elf_info_t kernel_elf_info;
  if (kernel_elf_parse_info(kernel_elf_buffer, kernel_elf_size, &kernel_elf_info) < 0) {
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
  if (virtual_addr_allocate_table(system_table, &table) < 0) {
    Print(L"Failed to allocate virtual address table: %r\r\n", status);
    return EFI_ABORTED;
  }

  // Load the kernel into memory and map into VA space.
  if (kernel_loader_load(system_table, &table, &kernel_elf_info) < 0) {
    Print(L"Failed to load kernel into memory: %r\r\n", status);
    return EFI_ABORTED;
  }

  // Allocate physical pages for the kernel stack.
  EFI_PHYSICAL_ADDRESS kernel_stack_physical_address = 0;
  status = system_table->BootServices->AllocatePages(
    AllocateAnyPages,
    EfiLoaderData,
    KERNEL_STACK_REGION_PAGES,
    &kernel_stack_physical_address
  );
  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate pages for kernel stack: %r\r\n", status);
    return EFI_ABORTED;
  }

  // Map the kernel stack into VA space.
  if (KERNEL_STACK_VIRTUAL_BASE % 16 != 0) {
    Print(L"Failed to map kernel stack into virtual address space, since it is not aligned to 16 bytes\r\n");
    return EFI_ABORTED;
  }
  if (virtual_addr_map(
    system_table,
    &table,
    kernel_stack_physical_address,
    KERNEL_STACK_VIRTUAL_BASE,
    KERNEL_STACK_REGION_PAGES,
    PAGE_FLAG_EL1_RW | PAGE_FLAG_PXN | PAGE_FLAG_UXN | PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_MAIR_ATTR(3ULL)
  ) < 0) {
    Print(L"Failed to map kernel stack into virtual address space: %r\r\n", status);
    return EFI_ABORTED;
  }

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
  SetMem(&boot_info, sizeof(boot_info_t), 0);

  boot_info.framebuffer = (uint32_t *)graphics_output_protocol->Mode->FrameBufferBase;
  boot_info.framebuffer_size = graphics_output_protocol->Mode->Info->HorizontalResolution * graphics_output_protocol->Mode->Info->VerticalResolution;
  boot_info.framebuffer_width = graphics_output_protocol->Mode->Info->HorizontalResolution;
  boot_info.framebuffer_stride = graphics_output_protocol->Mode->Info->HorizontalResolution * sizeof(uint32_t);

  Print(L"Framebuffer Base: 0x%lx\r\n", boot_info.framebuffer);
  Print(L"Framebuffer Size: 0x%lx\r\n", boot_info.framebuffer_size);
  Print(L"Framebuffer Width: 0x%lx\r\n", boot_info.framebuffer_width);
  Print(L"Framebuffer Stride: 0x%lx\r\n", boot_info.framebuffer_stride);

  for (int i = 0; i < system_table->NumberOfTableEntries; i++) {
    EFI_CONFIGURATION_TABLE *table = &system_table->ConfigurationTable[i];
    if (CompareGuid(&table->VendorGuid, &gEfiAcpiTableGuid)) {
      Print(L"ACPI table found at: 0x%r\r\n", table->VendorTable);
      boot_info.acpi_table = (void*)table->VendorTable;
    }
  }

  status = load_boot_modules(system_table, file_protocol, &boot_info);
  if (EFI_ERROR(status)) {
    Print(L"Failed to load boot modules: %r\r\n", status);
    return status;
  }

  for (int i = 0; i < boot_info.num_boot_modules; i++) {
    CHAR16 name[50];
    AsciiStrToUnicodeStrS(boot_info.boot_modules[i].name, name, 50);
    Print(L"Boot module %d: %s\r\n", i, name);
  }

  // ============================================ Copy the boot info to physical memory ============================================

  boot_info_t *boot_info_physical_address = NULL;
  status = system_table->BootServices->AllocatePool(
    EfiLoaderData,
    sizeof(boot_info_t),
    (VOID **)&boot_info_physical_address
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

  // Free the kernel buffer.
  system_table->BootServices->FreePool(kernel_elf_buffer);
  kernel_elf_buffer = NULL;

  // ============================================ Exit boot services ============================================
  status = exit_boot_services(image_handle, system_table, boot_info_physical_address);
  if (EFI_ERROR(status)) {
    Print(L"Failed to exit boot services: %r\r\n", status);
    return status;
  }

  // NOTE: NO MORE BOOT SERVICES CAN BE CALLED AFTER THIS POINT.

  // ============================================ Jump to the kernel ============================================

  // Enable MMU for TTBR1, using the VA table we've built.
  if (virtual_addr_apply_to_ttbr1(&table) < 0) {
    return -1;
  }

  uint64_t kernel_stack_top = KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_REGION_SIZE;
  handoff(kernel_stack_top, kernel_elf_info.entry_point, boot_info_physical_address);
}
