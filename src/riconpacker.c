/*******************************************************************************************
*
*   rIconPacker v1.0 - A simple and easy-to-use icon packer
*
*   CONFIGURATION:
*
*   #define ENABLE_PRO_FEATURES
*       Enable PRO features for the tool. Usually command-line and export options related.
*
*   DEPENDENCIES:
*       raylib 2.0              - Windowing/input management and drawing.
*       raygui 2.0              - IMGUI controls (based on raylib).
*       tinyfiledialogs 3.3.7   - Open/save file dialogs, it requires linkage with comdlg32 and ole32 libs.
*
*   COMPILATION (Windows - MinGW):
*       gcc -o riconpacker.exe riconpacker.c external/tinyfiledialogs.c -s riconpacker.rc.data -Iexternal / 
*           -lraylib -lopengl32 -lgdi32 -lcomdlg32 -lole32 -std=c99 -Wall
* 
*   COMPILATION (Linux - GCC):
*       gcc -o riconpacker riconpacker.c external/tinyfiledialogs.c -s -Iexternal -no-pie -D_DEFAULT_SOURCE /
*           -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
*
*   DEVELOPERS:
*       Ramon Santamaria (@raysan5):   Developer, supervisor, updater and maintainer.
*
*   LICENSE: Propietary License
*
*   Copyright (c) 2018 raylib technologies (@raylibtech). All Rights Reserved.
*   
*   Unauthorized copying of this file, via any medium is strictly prohibited
*   This project is proprietary and confidential unless the owner allows
*   usage in any other form by expresely written permission.
*
**********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "external/raygui.h"            // Required for: IMGUI controls

#include "external/tinyfiledialogs.h"   // Required for: Native open/save file dialogs

#include "external/stb_image.h"         // Required for: stbi_load_from_memory()
#include "external/stb_image_write.h"   // Required for: stbi_write_png_to_mem()

#include <stdio.h>                      // Required for: fopen(), fclose(), fread()...
#include <stdlib.h>                     // Required for: malloc(), free()
#include <string.h>                     // Required for: strcmp(), strlen()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#define ENABLE_PRO_FEATURES             // Enable PRO version features

#define TOOL_VERSION_TEXT       "1.0"   // Tool version string
#define MAX_DEFAULT_ICONS          8    // Number of icon images for embedding

//#define SUPPORT_RTOOL_GENERATION        // Support rTool icon generation

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Icon File Header (6 bytes)
typedef struct {
    unsigned short reserved;    // Must always be 0.
    unsigned short imageType;   // Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image. Other values are invalid.
    unsigned short icoPackCount;  // Specifies number of images in the file.
} IcoHeader;

// Icon Entry info (16 bytes)
typedef struct {
    unsigned char width;        // Specifies image width in pixels. Can be any number between 0 and 255. Value 0 means image width is 256 pixels.
    unsigned char height;       // Specifies image height in pixels. Can be any number between 0 and 255. Value 0 means image height is 256 pixels.
    unsigned char colpalette;   // Specifies number of colors in the color palette. Should be 0 if the image does not use a color palette.
    unsigned char reserved;     // Reserved. Should be 0.
    unsigned short planes;      // In ICO format: Specifies color planes. Should be 0 or 1.
                                // In CUR format: Specifies the horizontal coordinates of the hotspot in number of pixels from the left.
    unsigned short bpp;         // In ICO format: Specifies bits per pixel. [Notes 4]
                                // In CUR format: Specifies the vertical coordinates of the hotspot in number of pixels from the top.
    unsigned int size;          // Specifies the size of the image's data in bytes
    unsigned int offset;        // Specifies the offset of BMP or PNG data from the beginning of the ICO/CUR file
} IcoDirEntry;

// NOTE: All image data referenced by entries in the image directory proceed directly after the image directory. 
// It is customary practice to store them in the same order as defined in the image directory.

#if 0
//PNG file-format: https://en.wikipedia.org/wiki/Portable_Network_Graphics#File_format

// PNG Signature
unsigned char pngSign[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

// After signature we have a series of chunks, every chunck has the same structure:
typedef struct {
    unsigned int length;    // Big endian!
    unsigned char type[4];  // Chunck type: IDHR, PLTE, IDAT, IEND  /  gAMA, sRGB, tEXt, tIME...
    unsigned char *data;    // Chunck data
    unsigned int crc;       // 32bit CRC (computed over type and data)
} PNGChunk;

// A minimal PNG only requires: pngSign | PNGChunk(IHDR) | PNGChunk(IDAT) | PNGChunk(IEND)

// IHDR mandatory chunck: image info (13 bytes)
typedef struct {
    unsigned int width;         // Image width
    unsigned int height;        // Image width
    unsigned char bitDepth;     // Bit depth
    unsigned char colorType;    // Pixel format: 0 - Grayscale, 2 - RGB, 3 - Indexed, 4 - GrayAlpha, 6 - RGBA
    unsigned char compression;  // Compression method: 0
    unsigned char filter;       // Filter method: 0 (default)
    unsigned char interlace;    // Interlace scheme (optional): 0 (none)
} IHDRChunkData;
#endif

#if defined(SUPPORT_RTOOL_GENERATION)
// rTool icon generation
typedef struct {
    int size;
    int borderSize;
    char text[4];
    int textSize;
    Rectangle textRec;
    bool proVersion;
    Color color;
    Color *textPixels;      // In case text is not enough... useful for 32x32, 24x24, 16x16
} rToolIcon;
#endif

// One image entry for ico
typedef struct {
    int size;
    int valid;
    Image image;
    Texture texture;
} IconPackEntry;

// Icon platform type
typedef enum {
    ICON_PLATFORM_WINDOWS = 0,
    ICON_PLATFORM_FAVICON,
    ICON_PLATFORM_ANDROID,
    ICON_PLATFORM_IOS7,
} IconPlatform;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------

// NOTE: Default icon sizes by platform: http://iconhandbook.co.uk/reference/chart/
static int icoSizesWindows[8] = { 256, 128, 96, 64, 48, 32, 24, 16 };              // Windows app icons
static int icoSizesFavicon[10] = { 228, 152, 144, 120, 96, 72, 64, 32, 24, 16 };   // Favicon for multiple devices
static int icoSizesAndroid[10] = { 192, 144, 96, 72, 64, 48, 36, 32, 24, 16 };     // Android Launcher/Action/Dialog/Others icons, missing: 512
static int icoSizesiOS7[9] = { 152, 120, 80, 76, 58, 44, 40, 29, 22 };             // iOS7 App/Settings/Others icons, missing: 25, 50, 512

static int icoPackCount = 0;                // Icon images array counter
static IconPackEntry *icoPack;              // Icon images array

static int sizeListActive = 0;              // Current list text entry
static int sizeListCount = 0;               // Number of list text entries
char **sizeTextList = NULL;                 // Pointer to list text arrays
static int *icoSizesPlatform = NULL;        // Pointer to selected platform icon sizes

static int validCount = 0;                  // Valid ico images counter

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void ShowUsageInfo(void);            // Show command line usage info

static void InitIconPack(int platform);     // Initialize icon pack for an specific platform
static void RemoveIconPack(int num);        // Remove one icon from the pack

static void BtnExportIcon(IconPackEntry *icoPack, int count);
static void BtnExportImage(Image image);


static Image *LoadICO(const char *fileName, int *count);                // Load icon data
static void SaveICO(IconPackEntry *icoPack, int packCount, const char *fileName);  // Save icon data

// Check if provided image size has a valid index for current sizes scheme
static int CheckImageSize(int width, int height);

// Define png to memory write fuction
// NOTE: This function is internal to stb_image_write.h but not exposed by default
unsigned char *stbi_write_png_to_mem(unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

// Process image data to add raylib PRO version triangle
static void ImageTrianglePRO(Image *image, int offsetX, int offsetY, int triSize);

// Process image to add stegano-message embedded
static void ImageSteganoMessage(Image *image, const char *msg, int bytePadding);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    char inFileName[256] = { 0 };       // Input file name (required in case of drag & drop over executable)

    // Command-line usage mode
    //--------------------------------------------------------------------------------------
    if (argc > 1)
    {
        if (argc == 2)  // One file dropped over the executable or just one argument
        {
            // Check if it is a supported image file for simple viewing
            if (IsFileExtension(argv[1], ".avi") || 
                IsFileExtension(argv[1], ".png") || 
                IsFileExtension(argv[1], ".tga") || 
                IsFileExtension(argv[1], ".jpg") || 
                IsFileExtension(argv[1], ".dds"))
            {
                strcpy(inFileName, argv[1]);
            }
            else 
            {
                ShowUsageInfo();
                return 0;
            }
        }
#if defined(ENABLE_PRO_FEATURES)
        else
        {

        
            return 0;
        }
#endif
    }
    
    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 400;
    const int screenHeight = 380;
    
    SetTraceLog(0);                             // Disable trace log messsages
    //SetConfigFlags(FLAG_WINDOW_RESIZABLE);    // Window configuration flags
    InitWindow(screenWidth, screenHeight, FormatText("rIconPacker v%s - A simple and easy-to-use icons packer", TOOL_VERSION_TEXT));
    //SetExitKey(0);
    
    // General pourpose variables
    Vector2 mousePoint = { 0.0f, 0.0f };
    int framesCounter = 0;

    // File drop variables
    int dropsCount = 0;
    char **droppedFiles;
    
    // Exit variables
    bool exitWindow = false;
    bool closingWindowActive = false;
    
    // Initialize icon pack by platform
    InitIconPack(ICON_PLATFORM_WINDOWS);
    
#if defined(SUPPORT_RTOOL_GENERATION)
    // Initialize rTool icons for drawing
    rToolIcon rToolPack[8] = { 0 };
    
    for (int i = 0; i < 8; i++)
    {
        rToolPack[i].size = icoSizesWindows[i];
        rToolPack[i].borderSize = (int)ceil((float)rToolPack[i].size/16.0f);
        strcpy(rToolPack[i].text, "rIP\0");
        rToolPack[i].textSize = 50/(i + 1);         // TODO: Find a working formula > 50, 30, 20, 20, 10, 10, 10, 6?
        rToolPack[i].textRec.width = MeasureText(rToolPack[i].text, rToolPack[i].textSize);
        rToolPack[i].textRec.height = rToolPack[i].textSize;
        rToolPack[i].textRec.x = rToolPack[i].size - 2*rToolPack[i].borderSize - rToolPack[i].textRec.width;
        rToolPack[i].textRec.y = rToolPack[i].size - 2*rToolPack[i].borderSize - rToolPack[i].textRec.height;
        rToolPack[i].proVersion = false;
        rToolPack[i].color = DARKGRAY;
        
        // In case icon is smaller than 32x32 pixels, 
        // we generate a color array to fill
        if (rToolPack[i].size <= 32) 
        {
            rToolPack[i].textPixels = (Color *)malloc(rToolPack[i].size*rToolPack[i].size*sizeof(Color));
            for (int p = 0; p < rToolPack[i].size*rToolPack[i].size; p++) rToolPack[i].textPixels[p] = BLANK;
        }
    }
    
    bool pixelEditMode = false;
    
    RenderTexture2D iconTarget = LoadRenderTexture(256, 256);     // To draw icon and retrieve it?
    SetTextureFilter(iconTarget.texture, FILTER_POINT);
    
    //Image icon = GetTextureData(iconTarget.texture);
    //UpdateTexture(iconTarget.texture, const void *pixels);  
#endif
    
    // raygui: controls initialization
    //----------------------------------------------------------------------------------
    Vector2 anchor01 = { 0, 0 };

    int platformActive = 0;
    int prevPlatformActive = 0;
    const char *platformTextList[4] = { "Windows", "Favicon", "Android", "iOS 7" };
    
    // NOTE: GuiListView() variables need to be global

    int scaleAlgorythmActive = 1;
    const char *scaleAlgorythmTextList[2] = { "NearestN", "Bicubic" };
    //----------------------------------------------------------------------------------
    
    GuiSetStyleProperty(LISTVIEW_ELEMENTS_HEIGHT, 25);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!exitWindow)    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        framesCounter++;

        mousePoint = GetMousePosition();

        if (WindowShouldClose()) exitWindow = true;
        
        // Check for dropped files
        if (IsFileDropped())
        {
            int dropsCount = 0;   
            char **droppedFiles = GetDroppedFiles(&dropsCount);
            
            for (int i = 0; i < dropsCount; i++)
            {
                if (IsFileExtension(droppedFiles[i], ".ico"))
                {
                    // Load all ICO available images
                    int imCount = 0;
                    Image *images = LoadICO(droppedFiles[i], &imCount);
                    
                    for (int i = 0; i < imCount; i++)
                    {
                        // Validate loaded images to fit in available size slots
                        int index = CheckImageSize(images[i].width, images[i].height);
                        
                        if (index >= 0)     // Valid index image
                        {
                            // Re-load image from ico pack
                            UnloadImage(icoPack[index].image);
                            icoPack[index].image = ImageCopy(images[i]);
                            
                            UnloadTexture(icoPack[index].texture);
                            icoPack[index].texture = LoadTextureFromImage(icoPack[index].image);
                            
                            //icoPack[index].size = icoSizesPlatform[index];      // Not required
                            icoPack[index].valid = true;
                        }
                        else printf("WARNING: ICO contains not supported image size (%i x %i).", images[i].width, images[i].height);
                        
                        UnloadImage(images[i]);
                    }
                    
                    free(images);
                }
                else if (IsFileExtension(droppedFiles[i], ".png"))
                {
                    // Load dropped image and check if it's a valid image size
                    Image image = LoadImage(droppedFiles[i]);
                    
                    int index = CheckImageSize(image.width, image.height);

                    if (index >= 0)
                    {
                        // Force image to be RGBA, most icons could come as RGB
                        // TODO: Support RGB icon format
                        ImageFormat(&image, UNCOMPRESSED_R8G8B8A8);
                        
                        // Re-load image from ico pack
                        UnloadImage(icoPack[index].image);
                        icoPack[index].image = ImageCopy(image);

                        UnloadTexture(icoPack[index].texture);
                        icoPack[index].texture = LoadTextureFromImage(icoPack[index].image);
                        
                        //icoPack[index].size = icoSizesPlatform[index];    // Not required
                        icoPack[index].valid = true;
                    }
                    else 
                    {
                        printf("WARNING: PNG contains not supported image size (%i x %i).", image.width, image.height);
                        
                        // TODO: Re-scale image to the closer supported ico size
                    }
                    
                    UnloadImage(image);
                }
            }

            ClearDroppedFiles();
        }
        
        if (IsKeyPressed(KEY_DELETE))
        {
            if (sizeListActive == 0) for (int i = 0; i < icoPackCount; i++) RemoveIconPack(i);   // Delete all images in the series
            else RemoveIconPack(sizeListActive - 1);    // Delete one image
        }
        
        // Calculate valid images 
        validCount = 0;
        for (int i = 0; i < icoPackCount; i++) if (icoPack[i].valid) validCount++;
        
        
#if defined(SUPPORT_RTOOL_GENERATION)
        // Edit selected rTool icon text position and size
        if (sizeListActive > 0)
        {
            if (IsKeyPressed(KEY_RIGHT)) rToolPack[sizeListActive - 1].textRec.x++;
            else if (IsKeyPressed(KEY_LEFT)) rToolPack[sizeListActive - 1].textRec.x--;
            else if (IsKeyPressed(KEY_UP)) rToolPack[sizeListActive - 1].textRec.y--;
            else if (IsKeyPressed(KEY_DOWN)) rToolPack[sizeListActive - 1].textRec.y++;
            
            if (IsKeyDown(KEY_LEFT_CONTROL))
            {
                if (IsKeyPressed(KEY_UP)) rToolPack[sizeListActive - 1].textSize++;
                else if (IsKeyPressed(KEY_DOWN)) rToolPack[sizeListActive - 1].textSize--;
            }
        }

        // TODO: Pixel edit mode should be better redesigned (requires zoom and more!)
        if (IsKeyPressed(KEY_E)) pixelEditMode = !pixelEditMode;
        
        if (IsKeyPressed(KEY_SPACE))
        {
            /*
            width -> border
            256 -> 16 -> ok
            128 -> 8  -> ok
            96  -> 6  -> ok
            64  -> 4  -> ok
            48  -> 3  -> ok
            32  -> 2  -> ok
            24  -> 2  -> 1.5    !
            16  -> 2  -> 1      !
            */
            int triSize = icoPack[0].image.width/4;
            int borderWidth = (int)ceil((float)icoPack[0].image.width/16.0f);
            ImageTrianglePRO(&icoPack[0].image, icoPack[0].image.width - borderWidth*2 - triSize, borderWidth*2, triSize);
            
            UnloadTexture(icoPack[0].texture);
            icoPack[0].texture = LoadTextureFromImage(icoPack[0].image);
        }
        
        if (IsKeyPressed(KEY_K)) 
        {
            ImageSteganoMessage(&icoPack[0].image, "This is a test message!", 4);     // One char bit every 4 image bytes
        }
#endif
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            // raygui: controls drawing
            //----------------------------------------------------------------------------------
#if !defined(ENABLE_PRO_FEATURES)
            GuiDisable();
#endif
                platformActive = GuiComboBox((Rectangle){ anchor01.x + 10, anchor01.y + 10, 115, 25 }, platformTextList, 4, platformActive);
                
                if (platformActive != prevPlatformActive)
                {
                    InitIconPack(platformActive);
                    prevPlatformActive = platformActive;
                }
            GuiEnable();
            
            sizeListActive = GuiListView((Rectangle){ anchor01.x + 10, anchor01.y + 45, 115, 300 }, sizeTextList, sizeListCount, sizeListActive);
            
            // Draw dummy panel and border lines
            GuiDummyRec((Rectangle){ anchor01.x + 135, anchor01.y + 10, 256, 256 }, "");
            DrawRectangleLines(anchor01.x + 135, anchor01.y + 10, 256, 256, Fade(GRAY, 0.6f));
            
            if (sizeListActive == 0)
            {
                for (int i = 0; i < icoPackCount; i++) DrawTexture(icoPack[i].texture, anchor01.x + 135, anchor01.y + 10, WHITE);
            }
            else if (sizeListActive > 0)
            {
#if defined(SUPPORT_RTOOL_GENERATION)
                // Draw rTool generated icon
                DrawRectangle(anchor01.x + 135 + 128 - rToolPack[sizeListActive - 1].size/2,
                              anchor01.y + 10 + 128 - rToolPack[sizeListActive - 1].size/2,
                              rToolPack[sizeListActive - 1].size, rToolPack[sizeListActive - 1].size, RAYWHITE);
                DrawRectangleLinesEx((Rectangle){ anchor01.x + 135 + 128 - rToolPack[sizeListActive - 1].size/2,
                                                  anchor01.y + 10 + 128 - rToolPack[sizeListActive - 1].size/2,
                                                  rToolPack[sizeListActive - 1].size, rToolPack[sizeListActive - 1].size }, 
                                     rToolPack[sizeListActive - 1].borderSize, rToolPack[sizeListActive - 1].color);
                DrawText(rToolPack[sizeListActive - 1].text, 
                         anchor01.x + 135 + 128 - rToolPack[sizeListActive - 1].size/2 + rToolPack[sizeListActive - 1].textRec.x, 
                         anchor01.y + 10 + 128 - rToolPack[sizeListActive - 1].size/2 + rToolPack[sizeListActive - 1].textRec.y, 
                         rToolPack[sizeListActive - 1].textSize, rToolPack[sizeListActive - 1].color);
                
                DrawText(FormatText("%i", rToolPack[sizeListActive - 1].textSize), GetScreenWidth() - 50, 35, 10, RED);
                
                if (pixelEditMode && rToolPack[sizeListActive - 1].size <= 32)
                {
                    int size = rToolPack[sizeListActive - 1].size*8;
                    Vector2 cell = GuiGrid((Rectangle){ anchor01.x + 135 + 128 - size/2, anchor01.y + 10 + 128 - size/2, size, size }, 8, 1);
                    
                    if ((cell.x >= 0) && (cell.y >= 0))
                    {
                        DrawRectangleLines((int)(anchor01.x + 135 + 128 - size/2 + cell.x*8), (int)(anchor01.y + 10 + 128 - size/2 + cell.y*8), 8, 8, RED);
                    }
                }
#else
                DrawTexture(icoPack[sizeListActive - 1].texture, 
                            anchor01.x + 135 + 128 - icoPack[sizeListActive - 1].texture.width/2, 
                            anchor01.y + 10 + 128 - icoPack[sizeListActive - 1].texture.height/2, WHITE);
#endif
            }

            //GuiLabel((Rectangle){ anchor01.x + 135, anchor01.y + 270, 126, 25 }, "Scale algorythm:");
            //scaleAlgorythmActive = GuiComboBox((Rectangle){ anchor01.x + 135, anchor01.y + 295, 125, 25 }, scaleAlgorythmTextList, 2, scaleAlgorythmActive);
            
            if ((sizeListActive < 0) || (validCount == 0)) GuiDisable();
            
            if ((validCount == 0) || ((sizeListActive >= 0) && (!icoPack[sizeListActive - 1].valid))) GuiDisable();
            if (GuiButton((Rectangle){ anchor01.x + 135, anchor01.y + 275, 126, 25 }, "Remove"))
            {
                if (sizeListActive == 0) for (int i = 0; i < icoPackCount; i++) RemoveIconPack(i);   // Delete all images in the series
                else RemoveIconPack(sizeListActive - 1);    // Delete one image
            }
            GuiEnable();
            
            if (validCount == 0) GuiDisable();
            if (GuiButton((Rectangle){ anchor01.x + 265, anchor01.y + 275, 126, 25 }, "Generate"))
            {
                // Get bigger available icon
                int biggerValidSize = -1;
                for (int i = 0; i < icoPackCount; i++)
                {
                    if (icoPack[i].valid) { biggerValidSize = i; break; }
                }
                
                if (biggerValidSize >= 0)
                {
                    if (sizeListActive == 0) 
                    { 
                        // Generate all missing images in the series
                        for (int i = 0; i < icoPackCount; i++) 
                        {
                            if (!icoPack[i].valid)
                            {
                                UnloadImage(icoPack[i].image);
                                icoPack[i].image = ImageCopy(icoPack[biggerValidSize].image);
                                
                                if (scaleAlgorythmActive == 0) ImageResizeNN(&icoPack[i].image, icoPack[i].size, icoPack[i].size);
                                else if (scaleAlgorythmActive == 1) ImageResize(&icoPack[i].image, icoPack[i].size, icoPack[i].size);
                                
                                UnloadTexture(icoPack[i].texture);
                                icoPack[i].texture = LoadTextureFromImage(icoPack[i].image);

                                icoPack[i].valid = true;
                            }
                        }
                    }
                    else 
                    {
                        if (!icoPack[sizeListActive - 1].valid)
                        {
                            UnloadImage(icoPack[sizeListActive - 1].image);
                            icoPack[sizeListActive - 1].image = ImageCopy(icoPack[biggerValidSize].image);
                            
                            if (scaleAlgorythmActive == 0) ImageResizeNN(&icoPack[sizeListActive - 1].image, icoPack[sizeListActive - 1].size, icoPack[sizeListActive - 1].size);
                            else if (scaleAlgorythmActive == 1) ImageResize(&icoPack[sizeListActive - 1].image, icoPack[sizeListActive - 1].size, icoPack[sizeListActive - 1].size);
                            
                            UnloadTexture(icoPack[sizeListActive - 1].texture);
                            icoPack[sizeListActive - 1].texture = LoadTextureFromImage(icoPack[sizeListActive - 1].image);

                            icoPack[sizeListActive - 1].valid = true;
                        }
                    }
                }
            }
            GuiEnable();

            GuiLine((Rectangle){ anchor01.x + 135, anchor01.y + 305, 255, 10 }, 1);
            
            if ((validCount == 0) || (sizeListActive == 0) || ((sizeListActive >= 0) && (!icoPack[sizeListActive - 1].valid))) GuiDisable();
            if (GuiButton((Rectangle){ anchor01.x + 135, anchor01.y + 320, 125, 25 }, "Export Image")) 
            {
                // Export all available valid images
                //for (int i = 0; i < icoPackCount; i++) if (icoPack[i].valid) ExportImage(icoPack[i].image, FormatText("icon_%ix%i.png", icoPack[i].size, icoPack[i].size));
                
                if ((sizeListActive > 0) && (icoPack[sizeListActive - 1].valid)) BtnExportImage(icoPack[sizeListActive - 1].image);
            }
            GuiEnable();
            
            if (validCount == 0) GuiDisable();
            if (GuiButton((Rectangle){ anchor01.x + 265, anchor01.y + 320, 126, 25 }, "Export Icon")) BtnExportIcon(icoPack, icoPackCount);
            GuiEnable();

            // Draw status bar info
            if (sizeListActive == 0) 
            {
                int validCount = 0;
                for (int i = 0; i < icoPackCount; i++) if (icoPack[i].valid) validCount++;
                GuiStatusBar((Rectangle){ anchor01.x, anchor01.y + 355, 400, 25 }, FormatText("SELECTED: ALL  AVAILABLE: %i/%i", validCount, icoPackCount), 10);
            }
            else 
            {
                GuiStatusBar((Rectangle){ anchor01.x, anchor01.y + 355, 400, 25 }, (sizeListActive < 0) ? "" : FormatText("SELECTED: %ix%i  AVAILABLE: %i/1", icoPack[sizeListActive - 1].size, icoPack[sizeListActive - 1].size, icoPack[sizeListActive - 1].valid), 10);
            }

            GuiEnable();
            //----------------------------------------------------------------------------------

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    
    // Unload sizes text list
    for (int i = 0; i < sizeListCount; i++) free(sizeTextList[i]);
    free(sizeTextList);
    
    // Unload icon pack data
    for (int i = 0; i < icoPackCount; i++)
    {
        UnloadImage(icoPack[i].image);
        UnloadTexture(icoPack[i].texture);
    }
    
#if defined(SUPPORT_RTOOL_GENERATION)
    for (int i = 0; i < 8; i++) if (rToolPack[i].size <= 32) free(rToolPack[i].textPixels);
#endif
    
    free(icoPack);
    
    CloseWindow();      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}


//--------------------------------------------------------------------------------------------
// Module functions
//--------------------------------------------------------------------------------------------

// Show command line usage info
static void ShowUsageInfo(void)
{
    printf("\n//////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                              //\n");
    printf("// rIconPacker v%s - A simple and easy-to-use icons packer                     //\n", TOOL_VERSION_TEXT);
    printf("// powered by raylib v2.0 (www.raylib.com) and raygui v2.0                      //\n");
    printf("// more info and bugs-report: ray[at]raylibtech.com                             //\n");
    printf("//                                                                              //\n");
    printf("// Copyright (c) 2018 raylib technologies (@raylibtech)                         //\n");
    printf("//                                                                              //\n");
    printf("//////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("USAGE:\n\n");
    printf("    > riconpacker [--help] --input <filename.ext> [--output <filename.ext>]\n");
    printf("                  [--format <sample_rate> <sample_size> <channels>] [--play]\n");
    
    printf("\nOPTIONS:\n\n");
    printf("    -h, --help                      : Show tool version and command line usage help\n");
    printf("    -i, --input <filename.ext>      : Define input file.\n");
    printf("                                      Supported extensions: .rfx, .sfs, .wav\n");
    printf("    -o, --output <filename.ext>     : Define output file.\n");
    printf("                                      Supported extensions: .wav, .h\n");
    printf("                                      NOTE: If not specified, defaults to: output.wav\n\n");
    printf("    -f, --format <valu>             : Format output file.\n");
    printf("                                      Supported values:\n");
    printf("                                          Sample rate:      22050, 44100\n");
    printf("                                          Sample size:      8, 16, 32\n");
    printf("                                          Channels:         1 (mono), 2 (stereo)\n");
    printf("                                      NOTE: If not specified, defaults to: 44100, 16, 1\n\n");
    
    printf("\nEXAMPLES:\n\n");
    printf("    > rtoolname --input sound.rfx --output jump.wav\n");
    printf("        Process <sound.rfx> to generate <sound.wav> at 44100 Hz, 32 bit, Mono\n\n");
    printf("    > rtoolname --input sound.rfx --output jump.wav --format 22050 16 2\n");
    printf("        Process <sound.rfx> to generate <jump.wav> at 22050 Hz, 16 bit, Stereo\n\n");
    printf("    > rtoolname --input sound.rfx --play\n");
    printf("        Plays <sound.rfx>, wave data is generated internally but not saved\n\n");
}

// Initialize icon pack for an specific platform
static void InitIconPack(int platform)
{
    validCount = 0;
    sizeListActive = 0;
    
    int icoPlatformCount = 0;
    
    switch (platform)
    {
        case ICON_PLATFORM_WINDOWS: icoPlatformCount = 8; icoSizesPlatform = icoSizesWindows; break;
        case ICON_PLATFORM_FAVICON: icoPlatformCount = 10; icoSizesPlatform = icoSizesFavicon; break;
        case ICON_PLATFORM_ANDROID: icoPlatformCount = 10; icoSizesPlatform = icoSizesAndroid; break;
        case ICON_PLATFORM_IOS7: icoPlatformCount = 9; icoSizesPlatform = icoSizesiOS7; break;
        default: return;
    }
    
    // Unload previous sizes text list
    if ((sizeTextList != NULL) && (sizeListCount > 0))
    {
        printf("free(sizeTextList)\n");
        
        for (int i = 0; i < sizeListCount; i++) free(sizeTextList[i]);
        free(sizeTextList);
    }
    
    // Generate size text list using provided icon sizes
    sizeListCount = icoPlatformCount + 1;
    
    sizeTextList = (char **)malloc(sizeListCount*sizeof(char *));
    for (int i = 0; i < sizeListCount; i++)
    {
        sizeTextList[i] = (char *)malloc(64);   // 64 chars array
        if (i == 0) strcpy(sizeTextList[i], "ALL");
        else strcpy(sizeTextList[i], FormatText("%i x %i", icoSizesPlatform[i - 1], icoSizesPlatform[i - 1]));
    }

    // Unload previous icon pack
    if ((icoPack != NULL) && (icoPackCount > 0))
    {
        printf("free(icoPack)\n");
        
        for (int i = 0; i < icoPackCount; i++)
        {
            UnloadImage(icoPack[i].image);
            UnloadTexture(icoPack[i].texture);
        }
        
        free(icoPack);
    }
    
    icoPackCount = icoPlatformCount;
    icoPack = (IconPackEntry *)malloc(icoPackCount*sizeof(IconPackEntry));

    // Generate placeholder images
    for (int i = 0; i < icoPackCount; i++)
    {
        icoPack[i].size = icoSizesPlatform[i];
        
        icoPack[i].image = GenImageColor(icoPack[i].size, icoPack[i].size, DARKGRAY);
        ImageDrawRectangle(&icoPack[i].image, (Vector2){ 1, 1 }, (Rectangle){ 0, 0, icoPack[i].size - 2, icoPack[i].size - 2 }, GRAY);
        
        icoPack[i].texture = LoadTextureFromImage(icoPack[i].image);
        icoPack[i].valid = false;
    }
}

// Remove one icon from the pack
// NOTE: A placeholder image is re-generated
static void RemoveIconPack(int num)
{
    if (icoPack[num].valid)
    {
        UnloadImage(icoPack[num].image);
        UnloadTexture(icoPack[num].texture);
        
        icoPack[num].image = GenImageColor(icoPack[num].size, icoPack[num].size, DARKGRAY);
        ImageDrawRectangle(&icoPack[num].image, (Vector2){ 1, 1 }, (Rectangle){ 0, 0, icoPack[num].size - 2, icoPack[num].size - 2 }, GRAY);
        
        icoPack[num].texture = LoadTextureFromImage(icoPack[num].image);
        icoPack[num].valid = false;
    }
}

// Export icon, show proper save dialog
static void BtnExportIcon(IconPackEntry *icoPack, int count)
{
    char currentPathFile[256];

    // Add sample file name to currentPath
    strcpy(currentPathFile, GetWorkingDirectory());
    strcat(currentPathFile, "\\icon.ico\0");

    // Save file dialog
    const char *filters[] = { "*.ico" };
#if defined(_WIN32)
    const char *fileName = tinyfd_saveFileDialog("Save sound parameters file", currentPathFile, 1, filters, "Icon Files (*.ico)");
#elif defined(__linux__)
    const char *fileName = tinyfd_saveFileDialog("Save sound parameters file", "icon.ico", 1, filters, "Icon Files (*.ico)");
#endif

    if (fileName != NULL)
    {
        char outFileName[128] = { 0 };
        strcpy(outFileName, fileName);
        
        if (GetExtension(outFileName) == NULL) strcat(outFileName, ".ico\0");     // No extension provided
        if (outFileName != NULL) SaveICO(icoPack, count, outFileName); 
    }
}

static void BtnExportImage(Image image)
{
    // TODO: Get image filename

    ExportImage(image, FormatText("icon_%ix%i.png", image.width, image.height));
}

// Icon data loader
// NOTE: Returns an array of images
static Image *LoadICO(const char *fileName, int *count)
{
    Image *images = NULL;
    
    FILE *icoFile = fopen(fileName, "rb");
    
    // NOTE: raylib.ico, in the way it was generated, 256x256 PNG is embedded directly while the
    // resto of image sizes seem to be embedded in an uncompressed form (BMP?)

    // Load .ico information
    IcoHeader icoHeader = { 0 };
    fread(&icoHeader, 1, sizeof(IcoHeader), icoFile);

    printf("icoHeader.imageType: %i\n", icoHeader.imageType);
    printf("icoHeader.icoPackCount: %i\n", icoHeader.icoPackCount);
    
    images = (Image *)malloc(icoHeader.icoPackCount*sizeof(Image));
    *count = icoHeader.icoPackCount;
    
    IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.icoPackCount, sizeof(IcoDirEntry));
    unsigned char *icoData[icoHeader.icoPackCount];

    for (int i = 0; i < icoHeader.icoPackCount; i++)
    {
        fread(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);
        
        printf("%ix%i@%i - %i - %i\n", icoDirEntry[i].width, icoDirEntry[i].height, 
               icoDirEntry[i].bpp, icoDirEntry[i].size, icoDirEntry[i].offset);
    }
    
    for (int i = 0; i < icoHeader.icoPackCount; i++)
    {
        icoData[i] = (unsigned char *)malloc(icoDirEntry[i].size);
        fread(icoData[i], icoDirEntry[i].size, 1, icoFile);         // Read icon png data

        // Reading png data from memory buffer
        int channels;
        images[i].data = stbi_load_from_memory(icoData[i], icoDirEntry[i].size, &images[i].width, &images[i].height, &channels, 4);     // Force image data to 4 channels (RGBA) 
        
        images[i].mipmaps =  1;
        images[i].format = UNCOMPRESSED_R8G8B8A8;
        /*
        if (channels == 1) icoPack[i].image.format = UNCOMPRESSED_GRAYSCALE;
        else if (channels == 2) icoPack[i].image.format = UNCOMPRESSED_GRAY_ALPHA;
        else if (channels == 3) icoPack[i].image.format = UNCOMPRESSED_R8G8B8;
        else if (channels == 4) icoPack[i].image.format = UNCOMPRESSED_R8G8B8A8;
        else printf("WARNING: Number of data channels not supported");
        */
        printf("Read image data from PNG in memory: %ix%i @ %ibpp\n", images[i].width, images[i].height, channels*8);
    }

    fclose(icoFile);
    
    for (int i = 0; i < icoHeader.icoPackCount; i++) 
    {
        free(icoDirEntry);
        free(icoData[i]);
    }
    
    return images;
}

// Icon saver
static void SaveICO(IconPackEntry *icoPack, int packCount, const char *fileName)
{   
    for (int i = 0; i < packCount; i++)
    {
        if (icoPack[i].valid)
        {
            printf("SaveICO(): icoPac[%i].size = %i\n", i, icoPack[i].size);
            printf("SaveICO(): icoPac[%i].image.width = %i\n", i, icoPack[i].image.width);
            printf("SaveICO(): icoPac[%i].image.height = %i\n", i, icoPack[i].image.height);
            printf("SaveICO(): icoPac[%i].image.format = %i\n", i, icoPack[i].image.format);
        }
    }
    
    // Check icon pack for valid images
    int validCount = 0;
    for (int i = 0; i < packCount; i++) if (icoPack[i].valid) validCount++;
    
    printf("SaveICO(): validCount = %i\n", validCount);
    
    IcoHeader icoHeader = { .reserved = 0, .imageType = 1, .icoPackCount = validCount };
    IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.icoPackCount, sizeof(IcoDirEntry));
    
    unsigned char *icoData[icoHeader.icoPackCount];       // PNG files data
    int offset = 6 + 16*icoHeader.icoPackCount;
    
    for (int i = 0; i < packCount; i++)
    {
        // Check icon pack for valid images
        if (icoPack[i].valid)
        {
            int size = 0;     // Store generated png file size
            
            // Compress images data into PNG file data streams
            // TODO: Image data format could be RGB (3 bytes) instead of RGBA (4 bytes)
            icoData[i] = stbi_write_png_to_mem((unsigned char *)icoPack[i].image.data, icoPack[i].image.width*4, icoPack[i].image.width, icoPack[i].image.height, 4, &size);
            
            // NOTE 1: In-memory png could also be generated using miniz_tdef: tdefl_write_image_to_png_file_in_memory()
            // NOTE 2: miniz also provides a CRC32 calculation implementation

            icoDirEntry[i].width = (icoPack[i].size == 256) ? 0 : icoPack[i].size;
            icoDirEntry[i].height = (icoPack[i].size == 256) ? 0 : icoPack[i].size;
            icoDirEntry[i].bpp = 32;
            icoDirEntry[i].size = size;
            icoDirEntry[i].offset = offset;
            
            offset += size;
        }
    }
    
    FILE *icoFile = fopen(fileName, "wb");

    // Write ico header
    fwrite(&icoHeader, 1, sizeof(IcoHeader), icoFile);

    // Write icon images entries data
    for (int i = 0; i < icoHeader.icoPackCount; i++) fwrite(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

    // Write icon png data
    for (int i = 0; i < icoHeader.icoPackCount; i++) fwrite(icoData[i], 1, icoDirEntry[i].size, icoFile);

    fclose(icoFile);
    
    for (int i = 0; i < icoHeader.icoPackCount; i++)
    {
        free(icoDirEntry);
        free(icoData[i]);
    }
}

// Check if provided image size has a valid index for current sizes scheme
static int CheckImageSize(int width, int height)
{
    int index = -1;
    
    if (width != height) printf("WARNING: Image is not squared as expected (%i x %i).", width, height);
    else
    {
        for (int i = 0; i < icoPackCount; i++)
        {
            if (width == icoSizesPlatform[i]) { index = i; break; }
        }
    }
    
    return index;
}

// Process image data to add raylib PRO version triangle
// NOTE: Image should be squared
static void ImageTrianglePRO(Image *image, int offsetX, int offsetY, int triSize)
{
    Color *pixels = GetImageData(*image);

    for (int y = 0; y < triSize; y++)
    {
        for (int x = 0; x < triSize; x++)
        {
            if (x >= y) pixels[(offsetY + y)*image->width + (offsetX + x)] = pixels[0];
        }
    }

    UnloadImage(*image);
    *image = LoadImageEx(pixels, image->width, image->height);
    free(pixels);
}

// Process image to add stegano-message embedded
// NOTE: Every bytePadding, the LSB of every byte is used to embbed every bit of every character. 
// So, every 8x bytePadding, one character of the message is codified... 
// For a 64x64@32 image with a bytePadding = 4 (every pixel), you can embed: 64x64/8 = 512 characters
// NOTE: Only supports 8-bit per channel data: GRAYSCALE, GRAY+ALPHA, R8G8B8, R8G8B8A8
static void ImageSteganoMessage(Image *image, const char *msg, int bytePadding)
{
    #define BIT_SET(a,b) ((a) |= (1<<(b)))
    #define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
    #define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
    #define BIT_CHECK(a,b) ((a) & (1<<(b)))

    int bpp = 0;
    
    switch (image->format)
    {
        case UNCOMPRESSED_GRAYSCALE: bpp = 8; break;       // 8 bit per pixel (no alpha)
        case UNCOMPRESSED_GRAY_ALPHA: bpp = 16; break;     // 8*2 bpp (2 channels)
        case UNCOMPRESSED_R8G8B8: bpp = 24; break;         // 24 bpp
        case UNCOMPRESSED_R8G8B8A8: bpp = 32; break;       // 32 bpp
        default: break;
    }

    if (bpp == 0) printf("Image format not supported for steganography.\n");
    else
    {
        int j = 0, k = 0;           // Required to count characters and bits of every character byte
        int msgLen = strlen(msg);
        int imByteSize = image->width*image->height*bpp/8;
        
        printf("Image byte size: %i\n", imByteSize);
        printf("Message bit length: %i\n", msgLen*8);
        
        if ((imByteSize/bytePadding) < (msgLen*8)) printf("WARNING: Message does not fit on image.");
        
        for (int i = 0; i < imByteSize; i += bytePadding)
        {
            if (BIT_CHECK(msg[j], k) == 1) BIT_SET(((unsigned char *)image->data)[i], 7);
            
            k++;                                // Move to next bit of the character byte
            if (k >= 8) { k = 0; j++; }         // Move to the next character in the chars array
            
            if (j >= msgLen) i = imByteSize;    // Finish when full message gets embedded
        }
    }
}

/*
#define ICO_DATA_READER
#if defined(ICO_DATA_READER)
    if (IsFileExtension(argv[1], ".ico"))
    {
        // Icon data reader
        FILE *icoFile = fopen(argv[1], "rb");
        
        // NOTE: raylib.ico, in the way it was generated, 256x256 PNG is embedded directly while the
        // resto of image sizes seem to be embedded in an uncompressed form (BMP?)
        
        if (icoFile == NULL) return -1;

        // Load .ico information
        IcoHeader icoHeader = { 0 };
        fread(&icoHeader, 1, sizeof(IcoHeader), icoFile);

        printf("icoHeader.imageType: %i\n", icoHeader.imageType);
        printf("icoHeader.icoPackCount: %i\n", icoHeader.icoPackCount);
        
        IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.icoPackCount, sizeof(IcoDirEntry));
        unsigned char *icoData[icoHeader.icoPackCount];

        for (int i = 0; i < icoHeader.icoPackCount; i++)
        {
            fread(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);
            
            printf("%ix%i@%i - %i - %i\n", icoDirEntry[i].width, icoDirEntry[i].height, 
                   icoDirEntry[i].bpp, icoDirEntry[i].size, icoDirEntry[i].offset);
        }
        
        for (int i = 0; i < icoHeader.icoPackCount; i++)
        {
            icoData[i] = (unsigned char *)malloc(icoDirEntry[i].size);
            fread(icoData[i], icoDirEntry[i].size, 1, icoFile);         // Read icon png data

            Image image = { 0 };
            
            // Reading png data from memory buffer
            int channels;
            image.data = stbi_load_from_memory(icoData[i], icoDirEntry[i].size, &image.width, &image.height, &channels, 0);
            
            printf("Read image data from PNG in memory: %ix%i @ %ibpp\n", image.width, image.height, channels*8);
                           
            image.mipmaps =  1;
            
            if (channels == 1) image.format = UNCOMPRESSED_GRAYSCALE;
            else if (channels == 2) image.format = UNCOMPRESSED_GRAY_ALPHA;
            else if (channels == 3) image.format = UNCOMPRESSED_R8G8B8;
            else if (channels == 4) image.format = UNCOMPRESSED_R8G8B8A8;
            else printf("WARNING: Number of data channels not supported");
            
            // TODO: Extract icon name from dropped file

            ExportImage(image, FormatText("icon_%ix%i.png", image.width, image.height));
            
            UnloadImage(image);
        }

        fclose(icoFile);
        
        for (int i = 0; i < icoHeader.icoPackCount; i++) 
        {
            free(icoDirEntry);
            free(icoData[i]);
        }
    }
#endif

#if defined(ICO_DATA_WRITTER)
    // Generate ICO file manually
    char *icoFileNames[MAX_DEFAULT_ICONS];

    // Generate file names to load from expected sizes
    for (int i = 0; i < MAX_DEFAULT_ICONS; i++)
    {
        icoFileNames[i] = (char *)calloc(128, 1);   
        sprintf(icoFileNames[i], "%s_%ix%i.png", argv[1], icoSizesPlatform[i], icoSizesPlatform[i]);
    }
    
    FILE *icoFile = fopen(FormatText("%s.ico", argv[1]), "wb");
    
    IcoHeader icoHeader = { .reserved = 0, .imageType = 1, .icoPackCount = MAX_DEFAULT_ICONS };
    fwrite(&icoHeader, 1, sizeof(IcoHeader), icoFile);
    
    IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.icoPackCount, sizeof(IcoDirEntry));
    unsigned char *icoData[icoHeader.icoPackCount];
    int offset = 6 + 16*icoHeader.icoPackCount;
    
    for (int i = 0; i < icoHeader.icoPackCount; i++)
    {
        printf("Reading file: %s\n", icoFileNames[i]);
        FILE *pngFile = fopen(icoFileNames[i], "rb");
        
        fseek(pngFile, 0, SEEK_END);        // Move file pointer to end of file
        int size = (int)ftell(pngFile);     // Get file size
        fseek(pngFile, 0, SEEK_SET);        // Reset file pointer

        printf("File size: %i\n\n", size);
        
        icoData[i] = (unsigned char *)malloc(size);
        fread(icoData[i], size, 1, pngFile);   // Read fulll file data
        fclose(pngFile);

        icoDirEntry[i].width = (icoSizesPlatform[i] == 256) ? 0 : icoSizesPlatform[i];
        icoDirEntry[i].height = (icoSizesPlatform[i] == 256) ? 0 : icoSizesPlatform[i];
        icoDirEntry[i].bpp = 32;
        icoDirEntry[i].size = size;
        icoDirEntry[i].offset = offset;
        
        offset += size;
    }

    for (int i = 0; i < icoHeader.icoPackCount; i++) fwrite(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

    for (int i = 0; i < icoHeader.icoPackCount; i++) 
    {
        fwrite(icoData[i], 1, icoDirEntry[i].size, icoFile);
        printf("Data written: %i\n", icoDirEntry[i].size);
    }

    fclose(icoFile);

    for (int i = 0; i < MAX_DEFAULT_ICONS; i++) free(icoFileNames[i]);
    
    for (int i = 0; i < icoHeader.icoPackCount; i++) 
    {
        free(icoDirEntry);
        free(icoData[i]);
    }
*/
