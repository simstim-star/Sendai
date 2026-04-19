#pragma once
#include <ShObjIdl.h>

extern const COMDLG_FILTERSPEC GLTFModelsFilter[2];
extern const COMDLG_FILTERSPEC HDRModelsFilter[2];

PWSTR
Win32ShowFileDialog(COMDLG_FILTERSPEC *ModelsFilter, UINT ModelsFilterSize);