#pragma once
#include <ShObjIdl.h>

extern const COMDLG_FILTERSPEC GLTFModelsFilter[2];
extern const COMDLG_FILTERSPEC HDRModelsFilter[2];

PWSTR
Win32ShowFileDialog(const COMDLG_FILTERSPEC *const Filters, UINT FiltersSize);