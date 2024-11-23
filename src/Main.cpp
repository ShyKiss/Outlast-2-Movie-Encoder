#define cimg_verbosity 0

#include "CImg.h"
using namespace cimg_library;

#include <turbojpeg.h>
#include <lz4.h>
#include <vector>

typedef char ANSICHAR;
typedef float FLOAT;

#pragma pack(push, 1)

struct FOutlastMovieHeader
{
	ANSICHAR Header[8];
	DWORD Version;
	DWORD NumFrames;
	DWORD ResolutionX;
	DWORD ResolutionY;
	DWORD AudioOffset;
	DWORD AudioSize;
	DWORD MaxJpegSize;
};

struct FOutlastMovieFrame
{
	FLOAT FrameTime; 
	DWORD CameraMode;
	DWORD Offset;
	DWORD JpegSize;
	DWORD LZ4Size;
};

#pragma pack(pop)

void ShowHelp()
{
	printf("Usage OutlastMovieEncoder.exe [-q <JpegQuality>] [-f <FrameRate>] [-s <FrameSkip>] [-n <NumFrames>] <InputImage> <OutputMovie>\n\n");
	printf("  -q <JpegQuality> : JPEG Quality between 1 and 100 (Default = 96)\n");
	printf("  -f <FrameRate>   : Frame rate of the final video (Default = 30.0)\n");
	printf("  -s <FrameSkip>   : Processes every nth image, ignores the other (Default = 1)\n\n");
	printf("  -n <NumFrames>   : Number of frames to process, process all of them if zero. (Default = 0)\n\n");
	printf("  -j               : Dumps frames as JPEG on disk. Good for previewing.\n\n");
}

int GetInputFilenameFormat(const char* Input, char* Format)
{
	const char* Dot = strrchr(Input, '.');

	if (!Dot)
	{
		printf("Error, cannot parse input file name.\n\n");
		return -1;
	}

	if (Dot == Input || !isdigit(*(Dot - 1)))
	{
		printf("Error, input file name must end with a number.\n\n");
		return -1;
	}

	const char* FirstDigit = Dot;

	while (FirstDigit != Input && isdigit(*(FirstDigit - 1)))
	{
		FirstDigit--;
	}

	int NumDigits = (int)(Dot - FirstDigit);

	char NumberFormat[32] = "";
	sprintf(NumberFormat, "%%.0%dd", NumDigits);

	strncpy(Format, Input, FirstDigit - Input);
	strcat(Format, NumberFormat);
	strcat(Format, Dot);

	return atoi(FirstDigit);
}

unsigned char Clamp(float d)
{
	if(d < 0)
		return 0;

	if(d > 255)
		return 255;

	return (unsigned char)d;
}

// CRC 32 polynomial.
#define CRC32_POLY 0x04c11db7

DWORD GCRCTable[256];

void CRCTableInit()
{
	// Init CRC table.
	for( DWORD iCRC=0; iCRC<256; iCRC++ )
	{
		for( DWORD c=iCRC<<24, j=8; j!=0; j-- )
		{
			GCRCTable[iCRC] = c = c & 0x80000000 ? (c << 1) ^ CRC32_POLY : (c << 1);
		}
	}
}

DWORD appMemCrc( const void* InData, int Length, DWORD CRC = 0)
{
	unsigned char* Data = (unsigned char*)InData;
	CRC = ~CRC;
	for( int i=0; i<Length; i++ )
		CRC = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ Data[i]];
	return ~CRC;
}

void MergeMovies(const char* MergeMovie1, const char* MergeMovie2)
{
	FILE* f1 = fopen(MergeMovie1, "rb");
	FILE* f2 = fopen(MergeMovie2, "rb");

	if (f1 == NULL || f2 == NULL)
	{
		printf("Error opening movies.\n");
		return;
	}

	fseek(f1, 0L, SEEK_END);
	size_t Size1 = ftell(f1);
	fseek(f1, 0L, SEEK_SET);
	fseek(f2, 0L, SEEK_END);
	size_t Size2 = ftell(f2);
	fseek(f2, 0L, SEEK_SET);

	unsigned char* Data1 = (unsigned char*)malloc(Size1);
	unsigned char* Data2 = (unsigned char*)malloc(Size2);

	fread(Data1, 1, Size1, f1);
	fread(Data2, 1, Size2, f2);

	FOutlastMovieHeader* Header1 = (FOutlastMovieHeader*)Data1;
	FOutlastMovieHeader* Header2 = (FOutlastMovieHeader*)Data2;
	FOutlastMovieFrame* Movie1Frames = (FOutlastMovieFrame*)(Data1 + sizeof(FOutlastMovieHeader));
	FOutlastMovieFrame* Movie2Frames = (FOutlastMovieFrame*)(Data2 + sizeof(FOutlastMovieHeader));
	unsigned char* Movie1RawData = ((unsigned char*)Movie1Frames) + Header1->NumFrames * sizeof(FOutlastMovieFrame);
	unsigned char* Movie2RawData = ((unsigned char*)Movie2Frames) + Header2->NumFrames * sizeof(FOutlastMovieFrame); 

	FOutlastMovieHeader NewHeader = { 0 };

	memcpy(&NewHeader.Header[0], "OUTLAST2", 8);
	NewHeader.Version = 2;
	NewHeader.ResolutionX = Header1->ResolutionX;
	NewHeader.ResolutionY = Header1->ResolutionY;
	NewHeader.NumFrames = Header1->NumFrames + Header2->NumFrames;
	NewHeader.MaxJpegSize = __max(Header1->MaxJpegSize, Header2->MaxJpegSize);
	
	FILE* fm = fopen("Merge.ol2", "w+b");

	fwrite(&NewHeader, sizeof(NewHeader), 1, fm);

	for (int f = 0; f < Header1->NumFrames; f++)
	{
		FOutlastMovieFrame& Frame = Movie1Frames[f];
		Frame.Offset += Header2->NumFrames * sizeof(FOutlastMovieFrame);
	}

	fwrite(Movie1Frames, sizeof(FOutlastMovieFrame), Header1->NumFrames, fm);

	float Movie1Duration = Movie1Frames[Header1->NumFrames - 1].FrameTime + (Movie1Frames[Header1->NumFrames - 1].FrameTime - Movie1Frames[Header1->NumFrames - 2].FrameTime);
	DWORD Movie1PadSize  = Size1 - sizeof(FOutlastMovieHeader);

	for (int f = 0; f < Header2->NumFrames; f++)
	{
		FOutlastMovieFrame& Frame = Movie2Frames[f];
		Frame.FrameTime += Movie1Duration;
		Frame.Offset    += Movie1PadSize;
	}

	fwrite(Movie2Frames, sizeof(FOutlastMovieFrame), Header2->NumFrames, fm);
	fwrite(Movie1RawData, Size1 - (Movie1RawData - Data1), 1, fm);
	fwrite(Movie2RawData, Size2 - (Movie2RawData - Data2), 1, fm);

	fclose(f1);
	fclose(f2);
	fclose(fm);
}

int main(int argc, char** argv) 
{
	bool bShowHelp = false;
	bool bDumpJpeg = false;
	float FrameRate = 30.0f;
	int JpegQuality = 96;
	int FrameSkip = 1;
	int NumFrames = 0;
	const char* MergeMovie1 = NULL;
	const char* MergeMovie2 = NULL;

	if (argc >= 3)
	{
		for (int i = 1; i < argc - 2; i++) 
		{
			if (strcmp(argv[i],"-h") == 0) 
			{
				bShowHelp = true;
			}
			else if (strcmp(argv[i],"-j") == 0) 
			{
				bDumpJpeg = true;
			}
			else if (strcmp(argv[i],"-f")==0)
			{
				FrameRate = (float)atof(argv[++i]);
			}
			else if (strcmp(argv[i],"-q")==0)
			{
				JpegQuality = atoi(argv[++i]);
			}
			else if (strcmp(argv[i],"-s")==0)
			{
				FrameSkip = atoi(argv[++i]);
			}
			else if (strcmp(argv[i],"-n")==0)
			{
				NumFrames = atoi(argv[++i]);
			}
			else if (strcmp(argv[i],"-m")==0)
			{
				MergeMovie1 = argv[++i];
				MergeMovie2 = argv[++i];
			}
		}
	}
	else
	{
		bShowHelp = true;
	}

	if (bShowHelp)
	{
		ShowHelp();
		return 1;
	}

	CRCTableInit();

	if (MergeMovie1 && MergeMovie2)
	{
		MergeMovies(MergeMovie1, MergeMovie2);
		return -1;
	}

	char* InputFilename  = argv[argc - 2];
	char* OutputFilename = argv[argc - 1];

	char FileFormat[1024] = "";
	int  FileIndex = GetInputFilenameFormat(InputFilename, FileFormat);

	if (FileIndex < 0)
	{
		return 1;
	}

	FOutlastMovieHeader Header = { 0 };
	std::vector<FOutlastMovieFrame> Frames;
	std::vector<unsigned char> VideoData;
	std::vector<DWORD> FrameCRCs;

	tjhandle TurboJpegHandle = tjInitCompress();

	unsigned char* JpegBuffer = NULL;
	unsigned char* RGBA = NULL;
	unsigned char* CompressBuffer = NULL;

	int ResolutionX = -1;
	int ResolutionY = -1;

	int TotalSizeUncompressed = 0;
	int TotalSizeCompressed   = 0;

	do 
	{
		char ImageFileName[1024];
		sprintf(ImageFileName, FileFormat, FileIndex);

		try
		{
			CImg<unsigned char> Image(ImageFileName); 

			if (ResolutionX < 0 || ResolutionY < 0)
			{
				ResolutionX = Image._width;
				ResolutionY = Image._height;

				if (ResolutionX & 0x07 || ResolutionY & 0x07)
				{
					printf("Error, the input image resolution must be a multiple of 8.");
					return 1;
				}

				RGBA = new unsigned char[ResolutionX * ResolutionY * 4];

				int JpegBufferSize = tjBufSizeYUV2( ResolutionX, 4, ResolutionY, TJSAMP_420);
				JpegBuffer = tjAlloc(JpegBufferSize);
				CompressBuffer = new unsigned char[JpegBufferSize * 2];
			}
			else if (Image._width != ResolutionX || Image._height != ResolutionY)
			{
				printf("Error, the input images resolutions must all be the same.");
				return 1;
			}

			for (int yy = 0; yy < ResolutionY; yy++)
			{
				for (int xx = 0; xx < ResolutionX; xx++)
				{
					RGBA[yy * ResolutionX * 4 + xx * 4 + 0] = Image(xx, yy, 0, 0);
					RGBA[yy * ResolutionX * 4 + xx * 4 + 1] = Image(xx, yy, 0, 1);
					RGBA[yy * ResolutionX * 4 + xx * 4 + 2] = Image(xx, yy, 0, 2);
					RGBA[yy * ResolutionX * 4 + xx * 4 + 3] = 255;
				}
			}
					
			printf("...Processing frame %d.\n", Frames.size());

			unsigned long JpegSize = 0;
			if( tjCompress2( TurboJpegHandle, RGBA, ResolutionX, ResolutionX * 4, ResolutionY, TJPF_RGBX, &JpegBuffer, &JpegSize, TJSAMP_420, JpegQuality, TJFLAG_NOREALLOC | TJFLAG_ACCURATEDCT ) ) 
			{
				printf("tjCompressFromYUV error '%s'\n",  tjGetErrorStr() );
				return 1;
			}

			if (bDumpJpeg)
			{
				char JpegFilename[1024];
				strcpy(JpegFilename, ImageFileName);
				strcat(JpegFilename, ".jpg");
				FILE* ff = fopen(JpegFilename, "w+b");
				fwrite(JpegBuffer, JpegSize, 1, ff);
				fclose(ff);
			}

			DWORD CRC = appMemCrc(JpegBuffer, JpegSize);

			int IdenticalFrameNumber = -1;
			for (size_t i = 0; i < FrameCRCs.size(); i++)
			{
				if (FrameCRCs[i] == CRC && Frames[i].JpegSize == JpegSize)
				{
					IdenticalFrameNumber = (int)i;
				}
			}

			FOutlastMovieFrame Frame = { 0 };
		
			Frame.FrameTime = Frames.size() / (FLOAT)FrameRate;
			Frame.JpegSize = JpegSize;
			Frame.CameraMode = 0; // No camera effects.

			if (IdenticalFrameNumber >= 0)
			{
				printf("   Found identical frame: %d\n", IdenticalFrameNumber);

				Frame.Offset  = Frames[IdenticalFrameNumber].Offset;
				Frame.LZ4Size = Frames[IdenticalFrameNumber].LZ4Size;
			}
			else
			{
				Frame.Offset = VideoData.size();

				int CompressedSize = LZ4_compress((const char*)JpegBuffer, (char*)CompressBuffer, JpegSize);

				// How beneficial is LZ4 for this frame?
				if (CompressedSize / (FLOAT)JpegSize < 0.9f)
				{
					Frame.LZ4Size = CompressedSize;
					VideoData.insert(VideoData.end(), CompressBuffer, CompressBuffer + CompressedSize);
					Header.MaxJpegSize = __max(Header.MaxJpegSize, JpegSize);
				}
				else
				{
					VideoData.insert(VideoData.end(), JpegBuffer, JpegBuffer + JpegSize);
				}

				printf("   Uncompressed: %d, Compressed: %d\n", JpegSize, CompressedSize);

				TotalSizeUncompressed += JpegSize;
				TotalSizeCompressed   += CompressedSize;
			}

			Frames.push_back(Frame);
			FrameCRCs.push_back(CRC);

			if (NumFrames > 0 && Frames.size() == NumFrames)
			{
				break;
			}
		}
		catch (...)
		{
			break;
		}

		FileIndex += FrameSkip;
	} 
	while (true);

	for (size_t Idx = 0; Idx < Frames.size(); Idx++)
	{
		Frames[Idx].Offset += sizeof(FOutlastMovieHeader);
		Frames[Idx].Offset += sizeof(FOutlastMovieFrame) * Frames.size();
	}

	// Write final header and frames.
	memcpy(&Header.Header[0], "OUTLAST2", 8);
	Header.NumFrames = Frames.size();
	Header.Version = 2;
	Header.ResolutionX = ResolutionX;
	Header.ResolutionY = ResolutionY;

	FILE* f = fopen(OutputFilename, "w+b");

	if (f == NULL)
	{
		printf("Error, cannot open output file.");
		return 1;
	}

	fwrite(&Header, sizeof(FOutlastMovieHeader), 1, f);
	fwrite(&Frames[0], sizeof(FOutlastMovieFrame) * Frames.size(), 1, f);
	fwrite(&VideoData[0], VideoData.size(), 1, f);

	fclose(f);

	printf("TOTAL SIZE UNCOMPRESSED : %d\n", TotalSizeUncompressed);
	printf("TOTAL SIZE COMPRESSED   : %d\n", TotalSizeCompressed);

	return 0;
}