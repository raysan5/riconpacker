/*******************************************************************************************
*
*   rIconPacker v1.0 - A simple and easy-to-use icons packer
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

// Define png to memory write function
// NOTE: This function is internal to stb_image_write.h but not exposed by default
unsigned char *stbi_write_png_to_mem(unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

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
static int icoSizesiOS[9] = { 180, 152, 120, 87, 80, 76, 58, 40, 29 };             // iOS App/Settings/Others icons, missing: 512, 1024

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
#if defined(ENABLE_PRO_FEATURES)
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

// Load/Save/Export data functions
static void LoadIconPack(const char *fileName);     // Load icon file into icoPack
static Image *LoadICO(const char *fileName, int *count);    // Load icon data
static void SaveICO(IconPackEntry *icoPack, int packCount, const char *fileName);  // Save icon data

static void DialogLoadIcon(void);                   // Show dialog: load input file
static void DialogExportIcon(IconPackEntry *icoPack, int count);  // Show dialog: export icon file
static void DialogExportImage(Image image);         // Show dialog: export image file

// Icon pack management functions
static void InitIconPack(int platform);     // Initialize icon pack for an specific platform
static void RemoveIconPack(int num);        // Remove one icon from the pack

// Auxiliar functions
static int CheckImageSize(int width, int height);   // Check if provided image size has a valid index for current sizes scheme

#if defined(SUPPORT_RTOOL_GENERATION)
static void ImageTrianglePRO(Image *image, int offsetX, int offsetY, int triSize);  // Process image data to add raylib PRO version triangle
static void ImageSteganoMessage(Image *image, const char *msg, int bytePadding, int offset);    // Process image to add stegano-message embedded
#endif

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
        if ((argc == 2) &&  
            (strcmp(argv[1], "-h") != 0) && 
            (strcmp(argv[1], "--help") != 0))       // One argument (file dropped over executable?)
        {
            if (IsFileExtension(argv[1], ".ico"))
            {
                // Open file with graphic interface
                strcpy(inFileName, argv[1]);        // Read input filename
            }
        }
#if defined(ENABLE_PRO_FEATURES)
        else 
        {
            ProcessCommandLine(argc, argv);
            return 0;
        }
#endif      // ENABLE_PRO_FEATURES
    }
    
    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 400;
    const int screenHeight = 380;
    
    SetTraceLog(0);                             // Disable trace log messsages
    //SetConfigFlags(FLAG_WINDOW_RESIZABLE);    // Window configuration flags
    InitWindow(screenWidth, screenHeight, FormatText("rIconPacker v%s - A simple and easy-to-use icons packer", TOOL_VERSION_TEXT));
    //SetWindowMinSize(400, 380);
    //SetExitKey(0);
    
    // General pourpose variables
    Vector2 mousePoint = { 0.0f, 0.0f };
    int framesCounter = 0;

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
    
    int textEditMode = 0;       // 0 - Move/Scale text, 1 - Text pixels edit mode
    Vector2 cell = { -1, -1 };  // Grid cell mouse position
    
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
    const char *platformTextList[4] = { "Windows", "Favicon", "Android", "iOS" };
    
    // NOTE: GuiListView() variables need to be global

    int scaleAlgorythmActive = 1;
    const char *scaleAlgorythmTextList[2] = { "NearestN", "Bicubic" };
    //----------------------------------------------------------------------------------
    
    GuiSetStyleProperty(LISTVIEW_ELEMENTS_HEIGHT, 25);
    
    // Check if an icon input file has been provided on command line
    if (inFileName[0] != '\0') LoadIconPack(inFileName);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!exitWindow)    // Detect window close button or ESC key
    {
        // Dropped files logic
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            int dropsCount = 0;   
            char **droppedFiles = GetDroppedFiles(&dropsCount);
            
            for (int i = 0; i < dropsCount; i++)
            {
                if (IsFileExtension(droppedFiles[i], ".ico"))
                {
                    // Load icon images into IconPack
                    LoadIconPack(droppedFiles[i]);
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
        //----------------------------------------------------------------------------------
        
        // Keyboard shortcuts
        //------------------------------------------------------------------------------------
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) DialogLoadIcon();         // Show dialog: load input file (.ico, .png)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) DialogExportIcon(icoPack, icoPackCount);  // Show dialog: save icon file (.ico)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E)) 
        {
            if ((sizeListActive > 0) && (icoPack[sizeListActive - 1].valid)) DialogExportImage(icoPack[sizeListActive - 1].image); // Show dialog: export tool data (.ex1)
        }
#if defined(ENABLE_PRO_FEATURES)
        //if (IsKeyPressed(KEY_ONE)) GuiLoadStylePalette(paletteStyleLight);              // Load style color palette: light
        //if (IsKeyPressed(KEY_TWO)) GuiLoadStylePalette(paletteStyleDark);               // Load style color palette: dark
        //if (IsKeyPressed(KEY_THREE)) GuiLoadStylePalette(paletteStyleCandy);            // Load style color palette: candy
#endif
        if (IsKeyPressed(KEY_DELETE))
        {
            if (sizeListActive == 0) for (int i = 0; i < icoPackCount; i++) RemoveIconPack(i);  // Delete all images in the series
            else RemoveIconPack(sizeListActive - 1);                                            // Delete one image
        }
        //----------------------------------------------------------------------------------

        // Basic program flow logic
        //----------------------------------------------------------------------------------
        framesCounter++;                    // General usage frames counter
        mousePoint = GetMousePosition();    // Get mouse position each frame
        if (WindowShouldClose()) exitWindow = true;
        
        // Show closing window on ESC
        if (IsKeyPressed(KEY_ESCAPE))
        {
            // TODO: Define KEY_ESCAPE custom logic (i.e. Show save dialog)
            exitWindow = true;
        }
        //----------------------------------------------------------------------------------     

        // Calculate valid images 
        validCount = 0;
        for (int i = 0; i < icoPackCount; i++) if (icoPack[i].valid) validCount++;
        
#if defined(SUPPORT_RTOOL_GENERATION)
        // Choose text edit mode, edit text or edit pixels
        if (IsKeyPressed(KEY_E)) textEditMode = !textEditMode;

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

        if (textEditMode == 1)
        {
            // Pixels edit mode
            if ((rToolPack[sizeListActive - 1].size <= 32) && (cell.x >= 0) && (cell.y >= 0))
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    rToolPack[sizeListActive - 1].textPixels[(int)cell.x + (int)cell.y*rToolPack[sizeListActive - 1].size] = rToolPack[sizeListActive - 1].color;
                }
                else if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                {
                    rToolPack[sizeListActive - 1].textPixels[(int)cell.x + (int)cell.y*rToolPack[sizeListActive - 1].size] = BLANK;
                }
                
                // TODO: Update icon texture...
                // ISSUE: We are not drawing textures now, icon is just draw with basic shapes!
                //if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) UpdateTexture(texIcon, rToolPack[sizeListActive - 1].textPixels);
            }
        }

        // Add rTool ONE version triangle
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
        
        // Add stegano-message to the icon image
        if (IsKeyPressed(KEY_K)) 
        {
            ImageSteganoMessage(&icoPack[0].image, "This is a test message!", 4, 0);     // One char bit every 4 image bytes, no offset
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
                // Icon platform scheme selector
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
            
#if defined(SUPPORT_RTOOL_GENERATION)
            if (sizeListActive == 0)
            {
                // NOTE: On custom rTool icon edit mode, we don't need all icons drawn...
            }
            else if (sizeListActive > 0)
            {
                if ((textEditMode == 0) || (rToolPack[sizeListActive - 1].size > 32))       // Edit text position
                {
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
                }
                else if (textEditMode == 1)     // Edit text pixels painting
                {
                    // NOTE: Only small sizes supported for now
                    if (rToolPack[sizeListActive - 1].size <= 32)
                    {
                        int size = rToolPack[sizeListActive - 1].size;
                        int scaledSize = size*8;
                        
                        // Draw icon scaled for painting
                        DrawRectangle(anchor01.x + 135 + 128 - scaledSize/2, anchor01.y + 10 + 128 - scaledSize/2, scaledSize, scaledSize, RAYWHITE);
                        DrawRectangleLinesEx((Rectangle){ anchor01.x + 135 + 128 - scaledSize/2, anchor01.y + 10 + 128 - scaledSize/2, scaledSize, scaledSize }, 
                                             rToolPack[sizeListActive - 1].borderSize*8, rToolPack[sizeListActive - 1].color);
                                             
                        DrawText(rToolPack[sizeListActive - 1].text, 
                             anchor01.x + 135 + 128 - scaledSize/2 + rToolPack[sizeListActive - 1].textRec.x*8, 
                             anchor01.y + 10 + 128 - scaledSize/2 + rToolPack[sizeListActive - 1].textRec.y*8, 
                             rToolPack[sizeListActive - 1].textSize*10, Fade(rToolPack[sizeListActive - 1].color, 0.4f));
                        
                        // Draw grid (returns selected cell)
                        cell = GuiGrid((Rectangle){ anchor01.x + 135 + 128 - scaledSize/2, anchor01.y + 10 + 128 - scaledSize/2, scaledSize, scaledSize }, 8, 1);
                        
                        // Draw selected cell lines
                        if ((cell.x >= 0) && (cell.y >= 0)) DrawRectangleLines((int)(anchor01.x + 135 + 128 - scaledSize/2 + cell.x*8), (int)(anchor01.y + 10 + 128 - scaledSize/2 + cell.y*8), 8, 8, RED);
                        
                        // Draw pixel rectangles
                        for (int y = 0; y < size; y++)
                        {
                            for (int x = 0; x < size; x++)
                            {
                                DrawRectangle((int)(anchor01.x + 135 + 128 - scaledSize/2 + x*8), 
                                              (int)(anchor01.y + 10 + 128 - scaledSize/2 + y*8), 8, 8, 
                                              rToolPack[sizeListActive - 1].textPixels[y*size + x]);
                            }
                        }
                    }
                }
                
                DrawText(FormatText("%i", rToolPack[sizeListActive - 1].textSize), GetScreenWidth() - 50, 35, 10, RED);
            }
#else
            if (sizeListActive == 0)
            {
                for (int i = 0; i < icoPackCount; i++) DrawTexture(icoPack[i].texture, anchor01.x + 135, anchor01.y + 10, WHITE);
            }
            else if (sizeListActive > 0)
            {
                DrawTexture(icoPack[sizeListActive - 1].texture, 
                            anchor01.x + 135 + 128 - icoPack[sizeListActive - 1].texture.width/2, 
                            anchor01.y + 10 + 128 - icoPack[sizeListActive - 1].texture.height/2, WHITE);
            }
#endif

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
                
                if ((sizeListActive > 0) && (icoPack[sizeListActive - 1].valid)) DialogExportImage(icoPack[sizeListActive - 1].image);
            }
            GuiEnable();
            
            if (validCount == 0) GuiDisable();
            if (GuiButton((Rectangle){ anchor01.x + 265, anchor01.y + 320, 126, 25 }, "Export Icon")) DialogExportIcon(icoPack, icoPackCount);
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

#if defined(ENABLE_PRO_FEATURES)
// Show command line usage info
static void ShowCommandLineInfo(void)
{
    printf("\n//////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                              //\n");
    printf("// rIconPacker v%s - A simple and easy-to-use icons packer                     //\n", TOOL_VERSION_TEXT);
    printf("// powered by raylib v2.1 (www.raylib.com) and raygui v2.1                      //\n");
    printf("// more info and bugs-report: ray[at]raylibtech.com                             //\n");
    printf("//                                                                              //\n");
    printf("// Copyright (c) 2018 raylib technologies (@raylibtech)                         //\n");
    printf("//                                                                              //\n");
    printf("//////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("USAGE:\n\n");
    printf("    > riconpacker [--help] --input <file01.ext>,[file02.ext],... [--output <filename.ico>]\n");
    printf("                  [--platform <value>] [--generate <size01>,[size02],...]\n");
    
    printf("\nOPTIONS:\n\n");
    printf("    -h, --help                      : Show tool version and command line usage help\n");
    printf("    -i, --input <file01.ext>,[file02.ext],...\n");
    printf("                                    : Define input file(s). Comma separated for multiple files.\n");
    printf("                                      Supported extensions: .ico, .png\n");
    printf("    -o, --output <filename.ico>     : Define output file.\n");
    printf("                                      Supported extensions: .ico\n");
    printf("                                      NOTE: If not specified, defaults to: output.wav\n\n");
    printf("    -p, --platform <value>          : Define platform sizes scheme to support.\n");
    printf("                                      Supported values:\n");
    printf("                                          0 - Windows (Sizes: 256, 128, 96, 64, 48, 32, 24, 16)\n");          
    printf("                                          1 - Favicon (Sizes: 228, 152, 144, 120, 96, 72, 64, 32, 24, 16)\n");    
    printf("                                          2 - Android (Sizes: 192, 144, 96, 72, 64, 48, 36, 32, 24, 16)\n");
    printf("                                          3 - iOS (Sizes: 180, 152, 120, 87, 80, 76, 58, 40, 29)\n");
    printf("                                      NOTE: If not specified, any icon size can be generated\n\n");
    printf("    -g, --generate <size01>,[size02],...\n");
    printf("                                    : Define icon sizes to generate using input (bigger size available).\n");
    printf("                                      Comma separated for multiple generation sizes.\n");
    printf("                                      NOTE 1: Generated icons are always squared.\n\n");
    printf("                                      NOTE 2: If size is 0, generates all platform scheme missing sizes.\n\n");
    printf("    -s, --scale-algorythm <value>   : Define the algorythm used to scale images.\n");
    printf("                                      Supported values:\n");
    printf("                                          0 - Bicubic scaling algorythm (default)\n");          
    printf("                                          1 - Nearest-Neighbor scaling algorythm\n");
    
    printf("\nEXAMPLES:\n\n");
    printf("    > riconpacker --input sound.rfx --output jump.wav\n");
    printf("        Process <sound.rfx> to generate <sound.wav> at 44100 Hz, 32 bit, Mono\n\n");
    printf("    > riconpacker --input sound.rfx --output jump.wav --format 22050 16 2\n");
    printf("        Process <sound.rfx> to generate <jump.wav> at 22050 Hz, 16 bit, Stereo\n\n");
    printf("    > riconpacker --input sound.rfx --play\n");
    printf("        Plays <sound.rfx>, wave data is generated internally but not saved\n\n");
}

// Process command line input
static void ProcessCommandLine(int argc, char *argv[])
{
    // CLI required variables
    bool showUsageInfo = false;     // Toggle command line usage info
    
    char inFileName[256] = { 0 };   // Input file name
    char outFileName[256] = { 0 };  // Output file name
    int outputFormat = 0;           // Supported output formats

    // Process command line arguments
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showUsageInfo = true;
        }
        else if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--input") == 0))
        {
            // Check for valid argumment
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                int numInputFiles = 0;
                char **inputFiles = SplitText(argv[i + 1], ',', &numInputFiles);
                
                for (int f = 0; f < numInputFiles; f++)
                {
                    if (IsFileExtension(inputFiles[i], ".ico") ||
                        IsFileExtension(inputFiles[i], ".png"))
                    {
                        // TODO: Load input file?
                    }
                    else printf("WARNING: Input file extension not recognized: %s\n", inputFiles[i]);
                }
                
                // Free input files strings memory
                for (int f = 0; f < numInputFiles; f++) free(inputFiles[i]);
                if (inputFiles != NULL) free(inputFiles);
                
                i++;
            }
        }
        else if ((strcmp(argv[i], "-o") == 0) || (strcmp(argv[i], "--output") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-') && 
                (IsFileExtension(argv[i + 1], ".ico") || 
                 IsFileExtension(argv[i + 1], ".png"))) 
            {
                strcpy(outFileName, argv[i + 1]);   // Read output filename
                i++;
            }
            else printf("WARNING: Output file extension not recognized.\n");
        }
        else if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--platform") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                // TODO: Read provided platform value
            }
            else printf("WARNING: Platform provided not valid\n");
        }
    }
    
    // Process input files if provided
    if (inFileName[0] != '\0')
    {
        if (outFileName[0] == '\0') strcpy(outFileName, "output.ico");  // Set a default name for output in case not provided
        
        printf("\nInput file:       %s", inFileName);
        printf("\nOutput file:      %s", outFileName);
        printf("\nOutput format:    %i\n\n", 0);
        
        // TODO: Process input ---> output
    }

    if (showUsageInfo) ShowCommandLineInfo();
}
#endif      // ENABLE_PRO_FEATURES

//--------------------------------------------------------------------------------------------
// Load/Save/Export functions
//--------------------------------------------------------------------------------------------

// Load icon file into an image array
// NOTE: Operates on global variable: icoPack
static void LoadIconPack(const char *fileName)
{
    // Load all ICO available images
    int imCount = 0;
    Image *images = LoadICO(fileName, &imCount);
    
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

// Show dialog: load input file
static void DialogLoadIcon(void)
{
    // Open file dialog
    const char *filters[] = { "*.ico", "*.png" };
    const char *fileName = tinyfd_openFileDialog("Load icon or image file", "", 2, filters, "Sound Icon Files (*.ico, *.png)", 0);

    if (fileName != NULL)
    {
        // TODO: Load input file
    }
}

// Show dialog: save icon file
static void DialogExportIcon(IconPackEntry *icoPack, int count)
{
    // Save file dialog
    const char *filters[] = { "*.ico" };
    const char *fileName = tinyfd_saveFileDialog("Save icon file", "icon.ico", 1, filters, "Icon File (*.ico)");

    if (fileName != NULL)
    {
        char outFileName[128] = { 0 };
        strcpy(outFileName, fileName);
        
        // Check for valid extension and make sure it is
        if ((GetExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".ico")) strcat(outFileName, ".ico\0");
        
        SaveICO(icoPack, count, outFileName); 
    }
}

// Show dialog: export image file
static void DialogExportImage(Image image)
{
    // Save file dialog
    const char *filters[] = { "*.png" };
    const char *fileName = tinyfd_saveFileDialog("Save image file", FormatText("icon_%ix%i.png", image.width, image.height), 1, filters, "Image File (*.png)");

    if (fileName != NULL)
    {
        char outFileName[128] = { 0 };
        strcpy(outFileName, fileName);
        
        // Check for valid extension and make sure it is
        if ((GetExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".ico")) strcat(outFileName, ".ico\0");
        
        ExportImage(image, outFileName);
    }
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
        case ICON_PLATFORM_IOS7: icoPlatformCount = 9; icoSizesPlatform = icoSizesiOS; break;
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

#if defined(SUPPORT_RTOOL_GENERATION)
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
static void ImageSteganoMessage(Image *image, const char *msg, int bytePadding, int offset)
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
#endif

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
