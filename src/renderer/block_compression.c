#include "block_compression.h"
#include "render_types.h"

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

static BOOL HasTransparency(const UINT8 *Pixels, SIZE_T PixelCount);
static SIZE_T GetTotalBytes(const R_Texture *Source, const SIZE_T BlockBytes);
static VOID BlockCompressMip(UINT8 *Dst, const UINT8 *Pixels, INT W, INT H, BOOL HasAlpha);

BOOL
R_TryBlockCompressRGBA8(const R_Texture *Source, R_Texture *Out, UINT8 **OutBlob)
{
	BOOL IsR8G8B8A8_UNORM = Source->Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || Source->Format == DXGI_FORMAT_R8G8B8A8_UNORM;

	if (!IsR8G8B8A8_UNORM)
	{
		return FALSE;
	}

	if (Source->Width < 4 || Source->Height < 4 || Source->MipLevels == 0)
	{
		return FALSE;
	}

	for (UINT Mip = 0; Mip < Source->MipLevels; Mip++)
	{
		if (Source->MipPixels[Mip] == NULL)
		{
			return FALSE;
		}
	}

	BOOL HasAlpha = HasTransparency(Source->MipPixels[0], (SIZE_T)Source->Width * Source->Height);
	SIZE_T BlockBytes = HasAlpha ? 16 : 8;
	SIZE_T TotalBytes = GetTotalBytes(Source, BlockBytes);

	UINT8 *CompressedBlobStart = malloc(TotalBytes);
	if (CompressedBlobStart == NULL)
	{
		return FALSE;
	}

	*Out = *Source;
	INT W = Source->Width;
	INT H = Source->Height;
	UINT8 *CompressedBlobIterator = CompressedBlobStart;
	for (UINT Mip = 0; Mip < Source->MipLevels; Mip++)
	{
		BlockCompressMip(CompressedBlobIterator, (const UINT8 *)Source->MipPixels[Mip], W, H, HasAlpha);
		Out->MipPixels[Mip] = CompressedBlobIterator;
		CompressedBlobIterator += (SIZE_T)((W + 3) / 4) * ((H + 3) / 4) * BlockBytes;
		W = W > 1 ? W / 2 : 1;
		H = H > 1 ? H / 2 : 1;
	}

	const BOOL IsSRGB = Source->Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	if (HasAlpha)
	{
		Out->Format = IsSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
	}
	else
	{
		Out->Format = IsSRGB ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
	}

	*OutBlob = CompressedBlobStart;
	return TRUE;
}

BOOL
HasTransparency(const UINT8 *Pixels, SIZE_T PixelCount)
{
	for (SIZE_T i = 0; i < PixelCount; i++)
	{
		if (Pixels[i * 4 + 3] != 255)
		{
			return TRUE;
		}
	}
	return FALSE;
}

VOID
BlockCompressMip(UINT8 *Dest, const UINT8 *Pixels, INT W, INT H, BOOL HasAlpha)
{
	const INT NumsBlocksY = (H + 3) / 4;
	const INT NumBlocksX = (W + 3) / 4;
	UINT8 FlattenedCopyOfBlockRGBABytes[64];

	for (INT BlockY = 0; BlockY < NumsBlocksY; BlockY++)
	{
		for (INT BlockX = 0; BlockX < NumBlocksX; BlockX++)
		{
			for (INT CurrentBlockRow = 0; CurrentBlockRow < 4; CurrentBlockRow++)
			{
				INT AbsoluteRow = BlockY * 4 + CurrentBlockRow;
				AbsoluteRow = AbsoluteRow < H ? AbsoluteRow : H - 1;
				for (INT CurrentBlockCol = 0; CurrentBlockCol < 4; CurrentBlockCol++)
				{
					INT AbsoluteColumn = BlockX * 4 + CurrentBlockCol;
					AbsoluteColumn = AbsoluteColumn < W ? AbsoluteColumn : W - 1;
					const SIZE_T FlattenedOffset = (CurrentBlockRow * 4 + CurrentBlockCol) * 4;
					const SIZE_T RawDataOffset = ((SIZE_T)AbsoluteRow * W + AbsoluteColumn) * 4;
					memcpy(FlattenedCopyOfBlockRGBABytes + FlattenedOffset, Pixels + RawDataOffset, 4);
				}
			}
			stb_compress_dxt_block(Dest, FlattenedCopyOfBlockRGBABytes, HasAlpha, STB_DXT_NORMAL);
			Dest += HasAlpha ? 16 : 8;
		}
	}
}

SIZE_T
GetTotalBytes(const R_Texture *Source, const SIZE_T BlockBytes)
{
	SIZE_T TotalBytes = 0;
	INT W = Source->Width;
	INT H = Source->Height;

	for (UINT Mip = 0; Mip < Source->MipLevels; Mip++)
	{
		TotalBytes += (SIZE_T)((W + 3) / 4) * ((H + 3) / 4) * BlockBytes;
		W = W > 1 ? W / 2 : 1;
		H = H > 1 ? H / 2 : 1;
	}
	return TotalBytes;
}