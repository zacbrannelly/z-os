#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Protocol/GraphicsOutput.h>

EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutputProtocol;

void ClearScreen(UINT32 color) {
  GraphicsOutputProtocol->Blt(
    GraphicsOutputProtocol,
    (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)&color,
    EfiBltVideoFill,
    0,
    0,
    0,
    0,
    GraphicsOutputProtocol->Mode->Info->HorizontalResolution,
    GraphicsOutputProtocol->Mode->Info->VerticalResolution,
    0
  );
}

EFI_STATUS EFIAPI UefiMain (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
) {
  Print(L"Hello, Boss!\r\n");

  // EFI_HANDLE HandleBuffer;
  // EFI_HANDLE* Handles = NULL;
  // UINTN HandleCount;
  // EFI_STATUS status = SystemTable->BootServices->LocateHandle(
  //   ByProtocol,
  //   &gEfiGraphicsOutputProtocolGuid,
  //   NULL,
  //   &HandleCount,
  //   Handles
  // );

  // if (EFI_ERROR(status)) {
  //   Print(L"Failed to locate graphics output protocol\r\n");
  //   return status;
  // }

  // status = SystemTable->BootServices->OpenProtocol(
  //   HandleBuffer,
  //   &gEfiGraphicsOutputProtocolGuid,
  //   (VOID **)&GraphicsOutputProtocol,
  //   HandleBuffer,
  //   NULL,
  //   EFI_OPEN_PROTOCOL_GET_PROTOCOL
  // );

  // if (EFI_ERROR(status)) {
  //   Print(L"Failed to open graphics output protocol\r\n");
  //   return status;
  // }

  EFI_STATUS status = SystemTable->BootServices->LocateProtocol(
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    (VOID **)&GraphicsOutputProtocol
  );

  if (EFI_ERROR(status) || GraphicsOutputProtocol == NULL) {
    Print(L"Failed to locate graphics output protocol: %r\r\n", status);
    return status;
  }

  // Reserved, R, G, B
  UINT32 redColor = 0x00FFFF00;
  ClearScreen(redColor);

  UINTN modeInfoSize = 0;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *modeInfoBuffer = NULL;

  for (int i = 0; i < GraphicsOutputProtocol->Mode->MaxMode; i++) {
    status = GraphicsOutputProtocol->QueryMode(GraphicsOutputProtocol, i, &modeInfoSize, &modeInfoBuffer);
    if (EFI_ERROR(status)) {
      Print(L"Failed to query mode %d\r\n", i);
    } else {
      Print(L"Mode Size: %d\r\n", modeInfoSize);
      Print(L"Mode %d: %d x %d\r\n", i, modeInfoBuffer->HorizontalResolution, modeInfoBuffer->VerticalResolution);
    }
  }

  // while (TRUE) {
  //   Print(L"Select a mode: ");

  //   EFI_INPUT_KEY Key;

  //   SystemTable->ConIn->Reset(SystemTable->ConIn, FALSE);
  //   SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, NULL);
  //   SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);

  //   if (Key.UnicodeChar < '0' || Key.UnicodeChar > '9') {
  //     Print(L"Invalid input\r\n");
  //     continue;
  //   }

  //   UINT32 modeIndex = Key.UnicodeChar - '0';
  //   if (modeIndex >= GraphicsOutputProtocol->Mode->MaxMode) {
  //     Print(L"Invalid mode\r\n");
  //     continue;
  //   }

  //   status = GraphicsOutputProtocol->SetMode(GraphicsOutputProtocol, modeIndex);
  //   if (EFI_ERROR(status)) {
  //     Print(L"Failed to set mode %d\r\n", modeIndex);
  //     continue;
  //   }
    
  //   SystemTable->ConOut->Reset(SystemTable->ConOut, FALSE);
  //   SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
  //   ClearScreen(redColor);

  //   break;
  // }

  Print(L"Hello, Boss!\r\n");
  Print(L"Press any key...\r\n");

  UINTN memoryMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
  UINTN mapKey = 0;
  UINTN descriptorSize = 0;
  UINT32 descriptorVersion = 0;

  status = SystemTable->BootServices->GetMemoryMap(
    &memoryMapSize,
    memoryMap,
    &mapKey,
    &descriptorSize,
    &descriptorVersion
  );

  status = SystemTable->BootServices->AllocatePool(
    EfiLoaderData,
    memoryMapSize + 4096,
    (VOID **)&memoryMap
  );

  UINTN originalMemoryMapSize = memoryMapSize;
  memoryMapSize += 4096;

  Print(L"Memory Map Size: %d\r\n", originalMemoryMapSize);
  Print(L"Descriptor Size: %d\r\n", descriptorSize);

  if (EFI_ERROR(status)) {
    Print(L"Failed to allocate memory for memory map\r\n");
    return status;
  }

  status = SystemTable->BootServices->GetMemoryMap(
    &memoryMapSize,
    memoryMap,
    &mapKey,
    &descriptorSize,
    &descriptorVersion
  );

  if (EFI_ERROR(status)) {
    Print(L"Failed to get memory map\r\n");
    return status;
  }

  
  for (int i = 0; i < originalMemoryMapSize / descriptorSize; i++) {
    if (memoryMap[i].Type == EfiBootServicesCode) {
      Print(L"Memory Descriptor %d: 0x%x - 0x%x - %d - 0x%x\r\n", i, memoryMap[i].PhysicalStart, memoryMap[i].PhysicalStart + memoryMap[i].NumberOfPages * 4096, memoryMap[i].Type, memoryMap[i].VirtualStart);
    }
  }

  EFI_INPUT_KEY Key;

  SystemTable->ConIn->Reset(SystemTable->ConIn, FALSE);
  SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, NULL);
  SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);

  ClearScreen(0);

  return EFI_SUCCESS;
}