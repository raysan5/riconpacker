/*******************************************************************************************
*
*   rIconPacker v1.0 - A simple and easy-to-use icons packer
*
*   CONFIGURATION:
*
*   #define VERSION_ONE
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
#include <math.h>                       // Required for: ceil()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#define VERSION_ONE             // Enable PRO version features

#define TOOL_VERSION_TEXT       "1.0"   // Tool version string
#define MAX_DEFAULT_ICONS          8    // Number of icon images for embedding

#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

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
    unsigned short imageCount;  // Specifies number of images in the file.
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

// One image entry for ico
typedef struct {
    int size;                   // Icon size (squared)
    int valid;                  // Icon valid image generated/loaded
    Image image;                // Icon image
    Texture texture;            // Icon texture
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
static char **sizeTextList = NULL;          // Pointer to list text arrays
static int *icoSizesPlatform = NULL;        // Pointer to selected platform icon sizes

static int validCount = 0;                  // Valid ico images counter

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#if defined(VERSION_ONE)
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

// Load/Save/Export data functions
static void LoadIconPack(const char *fileName);             // Load icon file into icoPack
static Image *LoadICO(const char *fileName, int *count);    // Load icon data
static void SaveICO(Image *images, int imageCount, const char *fileName);  // Save icon data

static void DialogLoadIcon(void);                   // Show dialog: load input file
static void DialogExportIcon(IconPackEntry *icoPack, int count);  // Show dialog: export icon file
static void DialogExportImage(Image image);         // Show dialog: export image file

// Icon pack management functions
static void InitIconPack(int platform);             // Initialize icon pack for a specific platform
static void RemoveIconPack(int num);                // Remove one icon from the pack

// Auxiliar functions
static int CheckImageSize(int width, int height);   // Check if provided image size has a valid index for current sizes scheme

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
                strcpy(inFileName, argv[1]);        // Read input filename to open with gui interface
            }
        }
#if defined(VERSION_ONE)
        else
        {
            ProcessCommandLine(argc, argv);
            return 0;
        }
#endif      // VERSION_ONE
    }

    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 400;
    const int screenHeight = 380;

    SetTraceLogLevel(LOG_NONE);                 // Disable trace log messsages
    //SetConfigFlags(FLAG_WINDOW_RESIZABLE);    // Window configuration flags
    //SetConfigFlags(FLAG_MSAA_4X_HINT);        // Window configuration flags
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

    // raygui: controls initialization
    //----------------------------------------------------------------------------------
    Vector2 anchor01 = { 0, 0 };

    int platformActive = 0;
    int prevPlatformActive = 0;
    int scaleAlgorythmActive = 1;
    
    // NOTE: GuiListView() variables need to be global
    //----------------------------------------------------------------------------------

    GuiSetStyle(LISTVIEW, ELEMENTS_HEIGHT, 25);

    // Check if an icon input file has been provided on command line
    if (inFileName[0] != '\0') LoadIconPack(inFileName);

    SetTargetFPS(120);      // Increased for smooth pixel painting on edit mode
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!exitWindow)     // Detect window close button or ESC key
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

                        // Re-load image/texture from ico pack
                        UnloadImage(icoPack[index].image);
                        UnloadTexture(icoPack[index].texture);
                        
                        icoPack[index].image = ImageCopy(image);
                        icoPack[index].texture = LoadTextureFromImage(icoPack[index].image);
                        icoPack[index].size = icoSizesPlatform[index];
                        icoPack[index].valid = true;
                    }
                    else
                    {
                        printf("WARNING: PNG contains not supported image size (%i x %i).", image.width, image.height);

                        // TODO: Re-scale image to the closer supported ico size
                        //ImageResize(&image, newWidth, newHeight);
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

        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            // raygui: controls drawing
            //----------------------------------------------------------------------------------
#if !defined(VERSION_ONE)
            GuiDisable();
#endif
                // Icon platform scheme selector
                platformActive = GuiComboBox((Rectangle){ anchor01.x + 10, anchor01.y + 10, 115, 25 }, "Windows;Favicon;Android;iOS", platformActive);

                if (platformActive != prevPlatformActive)
                {
                    InitIconPack(platformActive);
                    prevPlatformActive = platformActive;
                }
            GuiEnable();

            GuiListView((Rectangle){ anchor01.x + 10, anchor01.y + 45, 115, 300 }, TextJoin(sizeTextList, sizeListCount, ";"), &sizeListActive, NULL, true);

            // Draw dummy panel and border lines
            GuiDummyRec((Rectangle){ anchor01.x + 135, anchor01.y + 10, 256, 256 }, "");
            DrawRectangleLines(anchor01.x + 135, anchor01.y + 10, 256, 256, Fade(GRAY, 0.6f));

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

            //GuiLabel((Rectangle){ anchor01.x + 135, anchor01.y + 270, 126, 25 }, "Scale algorythm:");
            //scaleAlgorythmActive = GuiComboBox((Rectangle){ anchor01.x + 135, anchor01.y + 295, 125, 25 }, "NearestN;Bicubic", scaleAlgorythmActive);

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

            GuiLine((Rectangle){ anchor01.x + 135, anchor01.y + 305, 255, 10 }, NULL);

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
                GuiStatusBar((Rectangle){ anchor01.x, anchor01.y + 355, 400, 25 }, FormatText("SELECTED: ALL  AVAILABLE: %i/%i", validCount, icoPackCount));
            }
            else
            {
                GuiStatusBar((Rectangle){ anchor01.x, anchor01.y + 355, 400, 25 }, (sizeListActive < 0) ? "" : FormatText("SELECTED: %ix%i  AVAILABLE: %i/1", icoPack[sizeListActive - 1].size, icoPack[sizeListActive - 1].size, icoPack[sizeListActive - 1].valid));
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

    free(icoPack);

    CloseWindow();      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}


//--------------------------------------------------------------------------------------------
// Module functions
//--------------------------------------------------------------------------------------------

#if defined(VERSION_ONE)
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
    printf("                                      Supported extensions: .png\n");
    printf("    -o, --output <filename.ico>     : Define output file.\n");
    printf("                                      Supported extensions: .ico\n");
    printf("                                      NOTE: If not specified, defaults to: output.ico\n\n");
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
    printf("    -x, --extract <file01.ico>      : Extract individual images from icon.\n");
    printf("                                      NOTE: Exported images naming: file01_{size}.png.\n\n");

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
    
    bool makeOne = false;           // Add ONE version triangle on corner

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
                if (IsFileExtension(argv[i + 1], ".ico")) strcpy(inFileName, argv[i + 1]);

                // WARNING: TextSplit() does not work!
                /*
                int numInputFiles = 0;
                char **inputFiles = TextSplit(argv[i + 1], ',', &numInputFiles);

                for (int f = 0; f < numInputFiles; f++)
                {
                    if (IsFileExtension(inputFiles[i], ".png"))
                    {
                        // TODO: Load input file?
                    }
                    else printf("WARNING: Input file extension not recognized: %s\n", inputFiles[i]);
                }

                // Free input files strings memory
                for (int f = 0; f < numInputFiles; f++) free(inputFiles[i]);
                if (inputFiles != NULL) free(inputFiles);
                */
                
                i++;
            }
        }
        else if ((strcmp(argv[i], "-o") == 0) || (strcmp(argv[i], "--output") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-') &&
                (IsFileExtension(argv[i + 1], ".ico")))
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
        else if (strcmp(argv[i], "--one") == 0) makeOne = true;         // raylib ONE triangle addition
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
#endif      // VERSION_ONE

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

        // Verify icon pack valid images (not placeholder ones)
        int validCount = 0;
        for (int i = 0; i < count; i++) if (icoPack[i].valid) validCount++;
        
        Image *images = (Image *)calloc(validCount, sizeof(Image));
        
        int imCount = 0;
        for (int i = 0; i < count; i++)
        {
            if (icoPack[i].valid)
            {
                images[imCount] = icoPack[i].image;
                imCount++;
            }
        }
        
        // Save valid images to output ICO file
        SaveICO(images, imCount, outFileName);
        
        free(images);
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
        ImageDrawRectangle(&icoPack[i].image, (Rectangle){ 1, 1, icoPack[i].size - 2, icoPack[i].size - 2 }, GRAY);

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
        ImageDrawRectangle(&icoPack[num].image, (Rectangle){ 1, 1, icoPack[num].size - 2, icoPack[num].size - 2 }, GRAY);
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

    // Load .ico information
    IcoHeader icoHeader = { 0 };
    fread(&icoHeader, 1, sizeof(IcoHeader), icoFile);

    //printf("icoHeader.imageType: %i\n", icoHeader.imageType);
    //printf("icoHeader.imageCount: %i\n", icoHeader.imageCount);

    images = (Image *)malloc(icoHeader.imageCount*sizeof(Image));
    *count = icoHeader.imageCount;

    IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.imageCount, sizeof(IcoDirEntry));
    unsigned char *icoData[icoHeader.imageCount];

    for (int i = 0; i < icoHeader.imageCount; i++) fread(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

    for (int i = 0; i < icoHeader.imageCount; i++)
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
        //printf("Read image data from PNG in memory: %ix%i @ %ibpp\n", images[i].width, images[i].height, channels*8);
    }

    fclose(icoFile);

    for (int i = 0; i < icoHeader.imageCount; i++)
    {
        free(icoDirEntry);
        free(icoData[i]);
    }

    return images;
}

// Apple ICNS icons loader
// NOTE: Check for reference: https://en.wikipedia.org/wiki/Apple_Icon_Image_format
static Image *LoadICNS(const char *fileName, int *count)
{
    Image *images = NULL;
    
    int icnsCount = 0;

    FILE *icnsFile = fopen(fileName, "rb");
    
    // Icns File Header (8 bytes)
    typedef struct {
        unsigned char id[4];        // Magic literal: "icns" (0x69, 0x63, 0x6e, 0x73)
        unsigned int size;          // Length of file, in bytes, msb first
    } IcnsHeader;

    // Icon Entry info (16 bytes)
    typedef struct {
        unsigned char type[4];      // Icon type, defined by OSType
        unsigned int dataSize;      // Length of data, in bytes (including type and length), msb first
        unsigned char *data;        // Icon data
    } IcnsData;

    // Load .icns information
    IcnsHeader icnsHeader = { 0 };
    fread(&icnsHeader, 1, sizeof(IcnsHeader), icnsFile);

    // TODO: Check file size to keep track of data read... until end of available data
    
    // TODO: Load all icons data found
    images = (Image *)malloc(icnsCount);
    *count = icnsCount;

    fclose(icnsFile);

    return images;
}

// Icon saver
// NOTE: Make sure images array sizes are valid!
static void SaveICO(Image *images, int imageCount, const char *fileName)
{
    IcoHeader icoHeader = { .reserved = 0, .imageType = 1, .imageCount = imageCount };
    IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.imageCount, sizeof(IcoDirEntry));

    unsigned char *icoData[icoHeader.imageCount];       // PNG files data
    int offset = 6 + 16*icoHeader.imageCount;

    for (int i = 0; i < imageCount; i++)
    {
        int size = 0;     // Store generated png file size

        // Compress images data into PNG file data streams
        // TODO: Image data format could be RGB (3 bytes) instead of RGBA (4 bytes)
        icoData[i] = stbi_write_png_to_mem((unsigned char *)images[i].data, images[i].width*4, images[i].width, images[i].height, 4, &size);

        // NOTE 1: In-memory png could also be generated using miniz_tdef: tdefl_write_image_to_png_file_in_memory()
        // NOTE 2: miniz also provides a CRC32 calculation implementation

        icoDirEntry[i].width = (images[i].width == 256) ? 0 : images[i].width;
        icoDirEntry[i].height = (images[i].width == 256) ? 0 : images[i].width;
        icoDirEntry[i].bpp = 32;
        icoDirEntry[i].size = size;
        icoDirEntry[i].offset = offset;

        offset += size;
    }

    FILE *icoFile = fopen(fileName, "wb");

    // Write ico header
    fwrite(&icoHeader, 1, sizeof(IcoHeader), icoFile);

    // Write icon images entries data
    for (int i = 0; i < icoHeader.imageCount; i++) fwrite(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

    // Write icon png data
    for (int i = 0; i < icoHeader.imageCount; i++) fwrite(icoData[i], 1, icoDirEntry[i].size, icoFile);

    fclose(icoFile);

    for (int i = 0; i < icoHeader.imageCount; i++)
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
