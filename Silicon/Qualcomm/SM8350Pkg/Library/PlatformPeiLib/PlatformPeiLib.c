/**
  Copyright (c) 2011-2014, ARM Limited. All rights reserved.
  Copyright (c) 2014, Linaro Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>

#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/DeviceConfigurationMapLib.h>
#include <Library/SerialPortLib.h>
#include <Library/MemoryMapHelperLib.h>

#include "PlatformPeiLib.h"

STATIC
EFI_STATUS
CfgGetMemInfoByName (
  CHAR8                           *RegionName,
  ARM_MEMORY_REGION_DESCRIPTOR_EX *MemRegions)
{
  return LocateMemoryMapAreaByName (RegionName, MemRegions);
}

STATIC
EFI_STATUS
CfgGetMemInfoByAddress (
  UINT64                           RegionBaseAddress,
  ARM_MEMORY_REGION_DESCRIPTOR_EX *MemRegions)
{
  return LocateMemoryMapAreaByAddress (RegionBaseAddress, MemRegions);
}

STATIC
EFI_STATUS
CfgGetCfgInfoString (
  CHAR8 *Key,
  CHAR8 *Value,
  UINTN *ValBuffSize)
{
  if (AsciiStriCmp (Key, "OsTypeString") == 0) {
    AsciiStrCpyS (Value, *ValBuffSize, "LA");
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
CfgGetCfgInfoVal (
  CHAR8  *Key,
  UINT32 *Value)
{
  PCONFIGURATION_DESCRIPTOR_EX ConfigurationDescriptorEx = GetDeviceConfigurationMap ();

  // Run through each Configuration Descriptor
  while (ConfigurationDescriptorEx->Value != 0xFFFFFFFF) {
    if (AsciiStriCmp (Key, ConfigurationDescriptorEx->Name) == 0) {
      *Value = (UINT32)(ConfigurationDescriptorEx->Value & 0xFFFFFFFF);
      return EFI_SUCCESS;
    }

    ConfigurationDescriptorEx++;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
CfgGetCfgInfoVal64 (
  CHAR8  *Key,
  UINT64 *Value)
{
  PCONFIGURATION_DESCRIPTOR_EX ConfigurationDescriptorEx = GetDeviceConfigurationMap ();

  // Run through each Configuration Descriptor
  while (ConfigurationDescriptorEx->Value != 0xFFFFFFFF) {
    if (AsciiStriCmp (Key, ConfigurationDescriptorEx->Name) == 0) {
      *Value = ConfigurationDescriptorEx->Value;
      return EFI_SUCCESS;
    }

    ConfigurationDescriptorEx++;
  }

  return EFI_NOT_FOUND;
}

STATIC
UINTN
SFlush (VOID) { return EFI_SUCCESS; }

STATIC
UINTN
SControl (
  IN UINTN Arg,
  IN UINTN Param)
{
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
SPoll (VOID) { return TRUE; }

STATIC
UINTN
SDrain (VOID) { return EFI_SUCCESS; }

STATIC
EFI_STATUS
ShInstallLib (
  IN CHAR8 *LibName,
  IN UINT32 LibVersion,
  IN VOID  *LibIntf)
{
  return EFI_SUCCESS;
}

UefiCfgLibType ConfigLib = {
  0x00010002,          CfgGetMemInfoByName,
  CfgGetCfgInfoString, CfgGetCfgInfoVal,
  CfgGetCfgInfoVal64,  CfgGetMemInfoByAddress
};

SioPortLibType SioLib = {
  0x00010001,          SerialPortRead,
  SerialPortWrite,     SPoll,
  SDrain,              SFlush,
  SControl,            SerialPortSetAttributes
};

STATIC
EFI_STATUS
ShLoadLib (
  IN  CHAR8 *LibName,
  IN  UINT32 LibVersion,
  OUT VOID **LibIntf)
{
  if (LibIntf == NULL) {
    return EFI_NOT_FOUND;
  }

  if (AsciiStriCmp (LibName, "UEFI Config Lib") == 0) {
    *LibIntf = &ConfigLib;
    return EFI_SUCCESS;
  }

  if (AsciiStriCmp (LibName, "SerialPort Lib") == 0) {
    *LibIntf = &SioLib;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

ShLibLoaderType ShLib = { 0x00010001, ShInstallLib, ShLoadLib };

STATIC
VOID
BuildMemHobForFv (IN UINT16 Type)
{
  EFI_PEI_HOB_POINTERS      HobPtr;
  EFI_HOB_FIRMWARE_VOLUME2 *Hob;

  HobPtr.Raw = GetHobList ();

  while ((HobPtr.Raw = GetNextHob (Type, HobPtr.Raw)) != NULL) {
    if (Type == EFI_HOB_TYPE_FV2) {
      Hob = HobPtr.FirmwareVolume2;

      // Build Memory Allocation HOB to mark it as BootServicesData
      BuildMemoryAllocationHob (Hob->BaseAddress, EFI_SIZE_TO_PAGES(Hob->Length) * EFI_PAGE_SIZE, EfiBootServicesData);
    }

    HobPtr.Raw = GET_NEXT_HOB(HobPtr);
  }
}

STATIC GUID gEfiInfoBlkHobGuid   = EFI_INFORMATION_BLOCK_GUID;
STATIC GUID gEfiShLibHobGuid     = EFI_SHIM_LIBRARY_GUID;
STATIC GUID gFvDecompressHobGuid = EFI_FV_DECOMPRESS_GUID;

VOID
InstallPlatformHob ()
{
  ARM_MEMORY_REGION_DESCRIPTOR_EX InfoBlk;
  ARM_MEMORY_REGION_DESCRIPTOR_EX UefiFd;
  LocateMemoryMapAreaByName ("Info Blk", &InfoBlk);
  LocateMemoryMapAreaByName ("UEFI FD",  &UefiFd);

  UINTN InfoBlkAddress      = InfoBlk.Address;
  UINTN ShLibAddress        = (UINTN)&ShLib;
  UINTN FvDecompressAddress = UefiFd.Address + 0x403D0;

  BuildGuidDataHob (&gEfiInfoBlkHobGuid,   &InfoBlkAddress,      sizeof(InfoBlkAddress));
  BuildGuidDataHob (&gEfiShLibHobGuid,     &ShLibAddress,        sizeof(ShLibAddress));
  BuildGuidDataHob (&gFvDecompressHobGuid, &FvDecompressAddress, sizeof(FvDecompressAddress));
}

EFI_STATUS
EFIAPI
PlatformPeim (VOID)
{
  BuildFvHob (PcdGet64(PcdFvBaseAddress), PcdGet32(PcdFvSize));

  BuildMemHobForFv (EFI_HOB_TYPE_FV2);

  InstallPlatformHob();

  return EFI_SUCCESS;
}
