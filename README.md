# Outlast Movie Encoder
Allows you to convert an array of `jpeg` pictures to `.ol2` video for [**Outlast 2**](https://store.steampowered.com/app/414700/Outlast_2) game

## Usage
Install [GraphicsMagick 1.3.32](https://sourceforge.net/projects/graphicsmagick/files/graphicsmagick-binaries/1.3.32/GraphicsMagick-1.3.32-Q16-win64-dll.exe/download)
`OutlastMovieEncoder.exe [-q <JpegQuality>] [-f <FrameRate>] [-s <FrameSkip>] [-n <NumFrames>] <InputImage> <OutputMovie>`

```
-q <JpegQuality> : JPEG Quality between 1 and 100 (Default = 96)
-f <FrameRate>   : Frame rate of the final video (Default = 30.0)
-s <FrameSkip>   : Processes every nth image, ignores the other (Default = 1)
-n <NumFrames>   : Number of frames to process, process all of them if zero. (Default = 0)
-j               : Dumps frames as JPEG on disk. Good for previewing.
```

## Compile

Install Install [GraphicsMagick 1.3.32](https://sourceforge.net/projects/graphicsmagick/files/graphicsmagick-binaries/1.3.32/GraphicsMagick-1.3.32-Q16-win64-dll.exe/download)
Install `VS120COMNTOOLS` and clone the repository

## Licenses
lz4 - [GPL-2.0-or-later](https://github.com/lz4/lz4/blob/dev/LICENSE) <p>
libjpeg-turbo - [IJG & BSD](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/LICENSE.md)
