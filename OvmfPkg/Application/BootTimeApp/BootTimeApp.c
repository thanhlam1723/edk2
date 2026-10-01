#include <Uefi.h>

#include <Guid/FirmwarePerformance.h>
#include <Guid/ExtendedFirmwarePerformance.h>

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PerformanceLib.h>

STATIC
VOID
PrintTimeMs (
  IN CONST CHAR16  *Name,
  IN UINT64        TimeNs
  )
{
  UINT64  Ms;
  UINT64  Fraction;

  Ms       = TimeNs / 1000000;
  Fraction = (TimeNs % 1000000) / 1000;

  Print (L"%-6s : %Lu.%03Lu ms\n", Name, Ms, Fraction);
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                                    Status;
  BOOT_PERFORMANCE_TABLE                       *BootTable;
  EFI_ACPI_5_0_FPDT_PERFORMANCE_RECORD_HEADER  *RecordHeader;
  FPDT_DYNAMIC_STRING_EVENT_RECORD              *StringRecord;

  UINT8   *RecordPtr;
  UINTN   Offset;
  UINTN   TableLength;
  UINTN   Index;

  UINT64  PeiStart;
  UINT64  PeiEnd;
  UINT64  DxeStart;
  UINT64  DxeEnd;
  UINT64  BdsStart;
  UINT64  BdsEnd;

  UINT64  PeiTime;
  UINT64  DxeTime;
  UINT64  BdsTime;
  UINT64  TotalTime;

  PeiStart = 0;
  PeiEnd   = 0;
  DxeStart = 0;
  DxeEnd   = 0;
  BdsStart = 0;
  BdsEnd   = 0;

  BootTable = NULL;

  Status = EfiGetSystemConfigurationTable (
             &gEdkiiFpdtExtendedFirmwarePerformanceGuid,
             (VOID **)&BootTable
             );

  if (EFI_ERROR (Status) || (BootTable == NULL)) {
    Print (L"Boot performance table not found: %r\n", Status);
    return EFI_NOT_FOUND;
  }

  TableLength = BootTable->Header.Length;

  Offset    = sizeof (BOOT_PERFORMANCE_TABLE);
  RecordPtr = (UINT8 *)BootTable + Offset;

  while (Offset < TableLength) {
    RecordHeader =
      (EFI_ACPI_5_0_FPDT_PERFORMANCE_RECORD_HEADER *)RecordPtr;

    if ((RecordHeader->Length == 0) ||
        ((Offset + RecordHeader->Length) > TableLength))
    {
      break;
    }

    if (RecordHeader->Type == FPDT_DYNAMIC_STRING_EVENT_TYPE) {
      StringRecord = (FPDT_DYNAMIC_STRING_EVENT_RECORD *)RecordHeader;

      //
      // PEI
      //
      if (AsciiStrCmp (StringRecord->String, "PEI") == 0) {
        if ((StringRecord->ProgressID == PERF_CROSSMODULE_START_ID) &&
            (PeiStart == 0))
        {
          PeiStart = StringRecord->Timestamp;
        }

        if ((StringRecord->ProgressID == PERF_CROSSMODULE_END_ID) &&
            (PeiEnd == 0))
        {
          PeiEnd = StringRecord->Timestamp;
        }
      }

      //
      // DXE
      //
      else if (AsciiStrCmp (StringRecord->String, "DXE") == 0) {
        if ((StringRecord->ProgressID == PERF_CROSSMODULE_START_ID) &&
            (DxeStart == 0))
        {
          DxeStart = StringRecord->Timestamp;
        }

        if ((StringRecord->ProgressID == PERF_CROSSMODULE_END_ID) &&
            (DxeEnd == 0))
        {
          DxeEnd = StringRecord->Timestamp;
        }
      }

      //
      // BDS - only use the first START and the first END after it.
      //
      //
// BDS START - use the first standard EDK2 BDS START.
//
else if (AsciiStrCmp (StringRecord->String, "BDS") == 0) {
  if ((StringRecord->ProgressID == PERF_CROSSMODULE_START_ID) &&
      (BdsStart == 0))
  {
    BdsStart = StringRecord->Timestamp;
  }
}

//
// BDS END for Boot Time Measurement.
// This marker is recorded immediately before BdsWait(),
// so user waiting time is not included.
//
else if (AsciiStrCmp (StringRecord->String, "BDS_BOOTMENU") == 0) {
  if ((StringRecord->ProgressID == PERF_CROSSMODULE_END_ID) &&
      (BdsStart != 0) &&
      (BdsEnd == 0))
  {
    BdsEnd = StringRecord->Timestamp;
  }
}
    }

    Offset    += RecordHeader->Length;
    RecordPtr += RecordHeader->Length;
  }

  if ((PeiStart == 0) || (PeiEnd == 0) ||
    (DxeStart == 0) || (DxeEnd == 0) ||
    (BdsStart == 0) || (BdsEnd == 0))
{
  Print (L"ERROR: Required boot phase records were not found.\n");
  Print (L"PEI Start : %Lu\n", PeiStart);
  Print (L"PEI End   : %Lu\n", PeiEnd);
  Print (L"DXE Start : %Lu\n", DxeStart);
  Print (L"DXE End   : %Lu\n", DxeEnd);
  Print (L"BDS Start : %Lu\n", BdsStart);
  Print (L"BDS End   : %Lu\n", BdsEnd);

  SystemTable->BootServices->Stall (10000000);
  return EFI_NOT_FOUND;
}

  PeiTime = PeiEnd - PeiStart;
  DxeTime = DxeEnd - DxeStart;
  BdsTime = BdsEnd - BdsStart;

  //
  // OVMF currently reports ResetEnd = 0.
  // FPDT performance timeline therefore uses zero as firmware baseline.
  //
  TotalTime = BdsEnd;

  Print (L"\n====================================\n");
  Print (L"       Firmware Boot Time\n");
  Print (L"====================================\n");

  PrintTimeMs (L"PEI", PeiTime);
  PrintTimeMs (L"DXE", DxeTime);
  PrintTimeMs (L"BDS", BdsTime);
  Print (L"------------------------------------\n");
  PrintTimeMs (L"Total", TotalTime);

  Print (L"====================================\n");

  Print (L"\nPress any key to exit...\n");

  SystemTable->BootServices->WaitForEvent (
                               1,
                               &SystemTable->ConIn->WaitForKey,
                               &Index
                               );

  return EFI_SUCCESS;
}