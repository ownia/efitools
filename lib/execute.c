/*
 * Copyright 2012 <James.Bottomley@HansenPartnership.com>
 *
 * see COPYING file
 *
 * --
 *
 * generate_path is a cut and paste from
 *  
 *   git://github.com/mjg59/shim.git
 *
 * Code Copyright 2012 Red Hat, Inc <mjg@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <efi.h>
#include <efilib.h>

#include <guid.h>
#include <execute.h>

EFI_STATUS
generate_path(CHAR16* name, EFI_LOADED_IMAGE *li, EFI_DEVICE_PATH **path, CHAR16 **PathName)
{
	EFI_DEVICE_PATH *devpath;
	EFI_HANDLE device;
	FILEPATH_DEVICE_PATH *FilePath;
	int len;
	unsigned int pathlen = 0;
	EFI_STATUS efi_status = EFI_SUCCESS;

	device = li->DeviceHandle;
	devpath = li->FilePath;

	while (!IsDevicePathEnd(devpath) &&
	       !IsDevicePathEnd(NextDevicePathNode(devpath))) {
		FilePath = (FILEPATH_DEVICE_PATH *)devpath;
		len = StrLen(FilePath->PathName);

		pathlen += len;

		if (len == 1 && FilePath->PathName[0] == '\\') {
			devpath = NextDevicePathNode(devpath);
			continue;
		}

		/* If no leading \, need to add one */
		if (FilePath->PathName[0] != '\\')
			pathlen++;

		/* If trailing \, need to strip it */
		if (FilePath->PathName[len-1] == '\\')
			pathlen--;

		devpath = NextDevicePathNode(devpath);
	}

	*PathName = AllocatePool((pathlen + 1 + StrLen(name))*sizeof(CHAR16));

	if (!*PathName) {
		Print(L"Failed to allocate path buffer\n");
		efi_status = EFI_OUT_OF_RESOURCES;
		goto error;
	}

	*PathName[0] = '\0';
	devpath = li->FilePath;

	while (!IsDevicePathEnd(devpath) &&
	       !IsDevicePathEnd(NextDevicePathNode(devpath))) {
		CHAR16 *tmpbuffer;
		FilePath = (FILEPATH_DEVICE_PATH *)devpath;
		len = StrLen(FilePath->PathName);

		if (len == 1 && FilePath->PathName[0] == '\\') {
			devpath = NextDevicePathNode(devpath);
			continue;
		}

		tmpbuffer = AllocatePool((len + 1)*sizeof(CHAR16));

		if (!tmpbuffer) {
			Print(L"Unable to allocate temporary buffer\n");
			return EFI_OUT_OF_RESOURCES;
		}

		StrCpy(tmpbuffer, FilePath->PathName);

		/* If no leading \, need to add one */
		if (tmpbuffer[0] != '\\')
			StrCat(*PathName, L"\\");

		/* If trailing \, need to strip it */
		if (tmpbuffer[len-1] == '\\')
			tmpbuffer[len-1] = '\0';

		StrCat(*PathName, tmpbuffer);
		FreePool(tmpbuffer);
		devpath = NextDevicePathNode(devpath);
	}

	if (name[0] != '\\')
		StrCat(*PathName, L"\\");
	StrCat(*PathName, name);

	*path = FileDevicePath(device, *PathName);

error:
	return efi_status;
}

EFI_STATUS
execute(EFI_HANDLE image, CHAR16 *name)
{
	EFI_STATUS status;
	EFI_HANDLE h;
	EFI_LOADED_IMAGE *li;
	EFI_DEVICE_PATH *devpath;
	CHAR16 *PathName;

	status = uefi_call_wrapper(BS->HandleProtocol, 3, image,
				   &IMAGE_PROTOCOL, &li);
	if (status != EFI_SUCCESS)
		return status;

	
	status = generate_path(name, li, &devpath, &PathName);
	if (status != EFI_SUCCESS)
		return status;

	Print(L"Generated path is %s\n", PathName);
	console_get_keystroke();

	status = uefi_call_wrapper(BS->LoadImage, 6, FALSE, image,
				   devpath, NULL, 0, &h);
	if (status != EFI_SUCCESS)
		return status;
	
	status = uefi_call_wrapper(BS->StartImage, 3, h, NULL, NULL);
	uefi_call_wrapper(BS->UnloadImage, 1, h);

	return status;
}
