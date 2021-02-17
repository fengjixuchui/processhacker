/*
 * Process Hacker -
 *   PE viewer
 *
 * Copyright (C) 2019-2021 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <peview.h>

//BOOLEAN PvpPeCheckImageDataEntryAddress(
//    _In_ ULONG Index,
//    _In_ ULONG StartRva,
//    _In_ ULONG EndRva
//    )
//{
//    PIMAGE_DATA_DIRECTORY directory;
//
//    for (ULONG i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++)
//    {
//        if (i == Index)
//            continue;
//
//        if (NT_SUCCESS(PhGetMappedImageDataEntry(&PvMappedImage, i, &directory)))
//        {
//            if ((StartRva >= directory->VirtualAddress) &&
//                (StartRva < directory->VirtualAddress + directory->Size))
//            {
//                return TRUE;
//            }
//
//            if ((EndRva >= directory->VirtualAddress) &&
//                (EndRva < directory->VirtualAddress + directory->Size))
//            {
//                return TRUE;
//            }
//        }
//    }
//
//    return FALSE;
//}

VOID PvpPeEnumerateImageDataDirectory(
    _In_ HWND ListViewHandle,
    _In_ ULONG Index,
    _In_ PWSTR Name
    )
{
    INT lvItemIndex;
    ULONG directoryAddress = 0;
    ULONG directorySize = 0;
    //BOOLEAN directoryOverlay = FALSE;
    PIMAGE_DATA_DIRECTORY directory;
    PIMAGE_SECTION_HEADER directorySection = NULL;
    WCHAR value[PH_INT64_STR_LEN_1];

    if (NT_SUCCESS(PhGetMappedImageDataEntry(&PvMappedImage, Index, &directory)))
    {
        if (directory->VirtualAddress)
        {
            directoryAddress = directory->VirtualAddress;
        }

        if (directory->Size)
        {
            directorySize = directory->Size;
        }

        if (directoryAddress)
        {
            directorySection = PhMappedImageRvaToSection(&PvMappedImage, directoryAddress);
        }

        //if (directoryAddress && directorySize)
        //{
        //    directoryOverlay = PvpPeCheckImageDataEntryAddress(
        //        Index,
        //        directoryAddress,
        //        PtrToUlong(PTR_ADD_OFFSET(directoryAddress, directorySize))
        //        );
        //}
    }

    PhPrintUInt32(value, Index + 1);
    lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, value, NULL);
    PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, Name);

    if (directoryAddress)
    {
        PhPrintPointer(value, UlongToPtr(directoryAddress));
        PhSetListViewSubItem(ListViewHandle, lvItemIndex, 2, value);
    }

    if (directorySize)
    {
        PhPrintPointer(value, PTR_ADD_OFFSET(directoryAddress, directorySize));
        PhSetListViewSubItem(ListViewHandle, lvItemIndex, 3, value);
        PhSetListViewSubItem(ListViewHandle, lvItemIndex, 4, PhaFormatSize(directorySize, ULONG_MAX)->Buffer);
    }

    if (directorySection)
    {
        WCHAR sectionName[IMAGE_SIZEOF_SHORT_NAME + 1];

        if (PhGetMappedImageSectionName(
            directorySection,
            sectionName,
            RTL_NUMBER_OF(sectionName),
            NULL
            ))
        {
            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 5, sectionName);
        }
    }

    if (directoryAddress && directorySize)
    {
        __try
        {
            PVOID directoryData;
            PH_HASH_CONTEXT hashContext;
            PPH_STRING hashString;
            UCHAR hash[32];

            if (directoryData = PhMappedImageRvaToVa(&PvMappedImage, directoryAddress, NULL))
            {
                PhInitializeHash(&hashContext, Md5HashAlgorithm);
                PhUpdateHash(&hashContext, directoryData, directorySize);

                if (PhFinalHash(&hashContext, hash, 16, NULL))
                {
                    if (hashString = PhBufferToHexString(hash, 16))
                    {
                        PhSetListViewSubItem(ListViewHandle, lvItemIndex, 6, hashString->Buffer);
                        PhDereferenceObject(hashString);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            PPH_STRING message;

            //message = PH_AUTO(PhGetNtMessage(GetExceptionCode()));
            message = PH_AUTO(PhGetWin32Message(RtlNtStatusToDosError(GetExceptionCode()))); // WIN32_FROM_NTSTATUS

            PhSetListViewSubItem(ListViewHandle, lvItemIndex, 6, PhGetStringOrEmpty(message));
        }
    }
}

typedef struct _PVP_PE_DIRECTORY_CONTEXT
{
    HWND WindowHandle;
    HWND ListViewHandle;
    HIMAGELIST ListViewImageList;
} PVP_PE_DIRECTORY_CONTEXT, *PPVP_PE_DIRECTORY_CONTEXT;

//COLORREF NTAPI PvPeSectionColorFunction(
//    _In_ INT Index,
//    _In_ PVOID Param,
//    _In_opt_ PVOID Context
//    )
//{
//    if ((BOOLEAN)Param)
//        return RGB(0xf0, 0xa0, 0xa0);
//    return RGB(0xff, 0xff, 0xff);
//}

INT_PTR CALLBACK PvpPeDirectoryDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    LPPROPSHEETPAGE propSheetPage;
    PPV_PROPPAGECONTEXT propPageContext;
    PPVP_PE_DIRECTORY_CONTEXT context;

    if (!PvPropPageDlgProcHeader(hwndDlg, uMsg, lParam, &propSheetPage, &propPageContext))
        return FALSE;

    if (uMsg == WM_INITDIALOG)
    {
        context = propPageContext->Context = PhAllocate(sizeof(PVP_PE_DIRECTORY_CONTEXT));
        memset(context, 0, sizeof(PVP_PE_DIRECTORY_CONTEXT));
    }
    else
    {
        context = propPageContext->Context;
    }

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->ListViewHandle = GetDlgItem(hwndDlg, IDC_LIST);

            PhSetListViewStyle(context->ListViewHandle, TRUE, TRUE);
            PhSetControlTheme(context->ListViewHandle, L"explorer");
            PhAddListViewColumn(context->ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 40, L"#");
            PhAddListViewColumn(context->ListViewHandle, 1, 1, 1, LVCFMT_LEFT, 130, L"Name");
            PhAddListViewColumn(context->ListViewHandle, 2, 2, 2, LVCFMT_LEFT, 100, L"RVA (start)");
            PhAddListViewColumn(context->ListViewHandle, 3, 3, 3, LVCFMT_LEFT, 100, L"RVA (end)");
            PhAddListViewColumn(context->ListViewHandle, 4, 4, 4, LVCFMT_LEFT, 100, L"Size");
            PhAddListViewColumn(context->ListViewHandle, 5, 5, 5, LVCFMT_LEFT, 100, L"Section");
            PhAddListViewColumn(context->ListViewHandle, 6, 6, 6, LVCFMT_LEFT, 100, L"Hash");
            PhSetExtendedListView(context->ListViewHandle);
            //ExtendedListView_SetItemColorFunction(context->ListViewHandle, PvPeSectionColorFunction);
            PhLoadListViewColumnsFromSetting(L"ImageDirectoryListViewColumns", context->ListViewHandle);

            if (context->ListViewImageList = ImageList_Create(2, 20, ILC_MASK | ILC_COLOR, 1, 1))
                ListView_SetImageList(context->ListViewHandle, context->ListViewImageList, LVSIL_SMALL);

            // for (ULONG i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++)
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_EXPORT, L"Export");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_IMPORT, L"Import");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_RESOURCE, L"Resource");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_EXCEPTION, L"Exception");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_SECURITY, L"Security");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_BASERELOC, L"Base relocation");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_DEBUG, L"Debug");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_ARCHITECTURE, L"Architecture");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_GLOBALPTR, L"Global PTR");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_TLS, L"TLS");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, L"Load configuration");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT, L"Bound imports");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_IAT, L"IAT");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT, L"Delay load imports");
            PvpPeEnumerateImageDataDirectory(context->ListViewHandle, IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR, L"CLR");

            PhInitializeWindowTheme(hwndDlg, PeEnableThemeSupport);
        }
        break;
    case WM_DESTROY:
        {
            PhSaveListViewColumnsToSetting(L"ImageDirectoryListViewColumns", context->ListViewHandle);

            if (context->ListViewImageList)
                ImageList_Destroy(context->ListViewImageList);

            PhFree(context);
        }
        break;
    case WM_SHOWWINDOW:
        {
            if (!propPageContext->LayoutInitialized)
            {
                PPH_LAYOUT_ITEM dialogItem;

                dialogItem = PvAddPropPageLayoutItem(hwndDlg, hwndDlg, PH_PROP_PAGE_TAB_CONTROL_PARENT, PH_ANCHOR_ALL);
                PvAddPropPageLayoutItem(hwndDlg, context->ListViewHandle, dialogItem, PH_ANCHOR_ALL);

                PvDoPropPageLayout(hwndDlg);

                propPageContext->LayoutInitialized = TRUE;
            }
        }
        break;
    case WM_NOTIFY:
        {
            PvHandleListViewNotifyForCopy(lParam, context->ListViewHandle);
        }
        break;
    case WM_CONTEXTMENU:
        {
            PvHandleListViewCommandCopy(hwndDlg, lParam, wParam, context->ListViewHandle);
        }
        break;
    }

    if (context)
    {
        REFLECT_MESSAGE_DLG(hwndDlg, context->ListViewHandle, uMsg, wParam, lParam);
    }

    return FALSE;
}
