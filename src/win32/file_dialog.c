#include "file_dialog.h"

static const COMDLG_FILTERSPEC ModelsFilter[] = {{L"glTF Models", L"*.gltf;*.glb"}, {L"All Files", L"*.*"}};

PWSTR
SelectGLTFPath(void)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) {
		return NULL;
	}

	IFileOpenDialog *FileOpenDialog = NULL;
	hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL, &IID_IFileOpenDialog, (void **)&FileOpenDialog);
	if (FAILED(hr)) {
		CoUninitialize();
		return NULL;
	}

	IFileDialog_SetFileTypes(FileOpenDialog, ARRAYSIZE(ModelsFilter), ModelsFilter);

	hr = IFileDialog_Show(FileOpenDialog, NULL);
	if (FAILED(hr)) {
		IFileDialog_Release(FileOpenDialog);
		CoUninitialize();
		return NULL;
	}

	IShellItem *ChosenFile = NULL;
	hr = IFileDialog_GetResult(FileOpenDialog, &ChosenFile);
	if (FAILED(hr)) {
		IFileDialog_Release(FileOpenDialog);
		CoUninitialize();
		return NULL;
	}

	PWSTR FilePath = NULL;
	hr = IShellItem_GetDisplayName(ChosenFile, SIGDN_FILESYSPATH, &FilePath);

	IShellItem_Release(ChosenFile);
	IFileDialog_Release(FileOpenDialog);
	CoUninitialize();

	if (FAILED(hr)) {
		return NULL;
	}

	return FilePath;
}