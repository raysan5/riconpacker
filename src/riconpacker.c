/*******************************************************************************************
*
*   rIconPacker v3.1 - A simple and easy-to-use icons packer and extractor
*
*   FEATURES:
*       - Pack icon images into icon file (.ico, .icns)
*       - Input image formats supported: .png, .bmp, .qoi
*       - Multiple platform templates for icon files
*       - Platform updated automatically from icon bucket
*       - Generate missing icon sizes automatically
*       - Define custom text data per icon image: icon-poems
*       - Extract and export icon images as .png files
*       - WEB: Download exported images as a .zip file
*
*   LIMITATIONS:
*       - Supports only .ico/.icns files containing .png image data (import/export)
*       - Supports only several OSTypes for .icns image files (modern OSTypes)
*
*   POSSIBLE IMPROVEMENTS:
*       - Support any-size input images, scaled to closest size
*       - CLI: Support custom text per icon
*
*   CONFIGURATION:
*       #define COMMAND_LINE_ONLY
*           Compile tool only for command line usage
*
*       #define CUSTOM_MODAL_DIALOGS
*           Use custom raygui generated modal dialogs instead of native OS ones
*           NOTE: Avoids including tinyfiledialogs depencency library
*
*   VERSIONS HISTORY:
*       3.1  (06-Apr-2024)  ADDED: Issue report window
*                           REMOVED: Sponsors window
*                           REVIEWED: Main toolbar and help window
*                           UPDATED: Using raylib 5.1-dev and raygui 4.1-dev
*
*       3.0  (19-Sep-2023)  **RE-RELEASE**
*                           ADDED: Support screen scaling x2
*                           UPDATED: Using raygui 4.0, latest GuiTextBox() features
*
*       3.0  (28-May-2023)  ADDED: Support macOS builds (x86_64 + arm64)
*                           ADDED: New platform template: macOS
*                           ADDED: Support for load/save .icns files
*                           ADDED: Icon-poem window on icon text loading
*                           ADDED: SaveImages() to export .png image pack
*                           REMOVED: Input file format .jpg
*                           REVIEWED: Updated UI to raygui 4.0
*                           REVIEWED: SaveICO(), avoid ico/image export at one
*                           REVIEWED: Image packaging into a single .zip not default
*                           REVIEWED: Command-line interface, using icon bucket
*                           REDESIGNED: Structure to support icon bucket
*                           REDESIGNED: Using raygui 4.0-dev
*
*       2.2  (13-Dec-2022)  ADDED: Welcome window with sponsors info
*                           REDESIGNED: Main toolbar to add tooltips
*                           REVIEWED: Help window implementation
*
*       2.1  (10-Oct-2022)  ADDED: Sponsor window for tools support
*                           Updated to raylib 4.5-dev and raygui 3.5-dev
*
*       2.0  (04-Oct-2022)  ADDED: Support text info data for every icon image
*                           ADDED: Export icon images as a .zip package
*                           ADDED: Main toolbar to access File/Tools/Visual options
*                           ADDED: Help window with keyboard shortcuts info
*                           ADDED: Multiple new gui styles
*                           REDESIGNED: UI to follow raylibtech UI conventions
*                           Updated to raylib 4.2 and raygui 3.2
*                           Source code re-licensed to open-source
*
*       1.5  (30-Dec-2021)  Updated to raylib 4.0 and raygui 3.1
*       1.0  (23-Mar-2019)  First release
*
*   DEPENDENCIES:
*       raylib 5.5-dev          - Windowing/input management and drawing
*       raygui 4.5-dev          - Immediate-mode GUI controls with custom styling and icons
*       rpng 1.5                - PNG chunks management
*       tinyfiledialogs 3.20    - Open/save file dialogs, it requires linkage with comdlg32 and ole32 libs
*       miniz 2.2.0             - Save .zip package file (required for multiple images export)
*
*   BUILDING:
*     - Windows (MinGW-w64):
*       gcc -o riconpacker.exe riconpacker.c external/tinyfiledialogs.c -s riconpacker.rc.data -Iexternal /
*           -lraylib -lopengl32 -lgdi32 -lcomdlg32 -lole32 -std=c99 -Wall
*
*     - Linux (GCC):
*       gcc -o riconpacker riconpacker.c external/tinyfiledialogs.c -s -Iexternal -no-pie -D_DEFAULT_SOURCE /
*           -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
*
*   DEVELOPERS:
*       Ramon Santamaria (@raysan5):   Developer, supervisor, updater and maintainer
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2018-2025 raylib technologies (@raylibtech) / Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#define TOOL_NAME               "rIconPacker"
#define TOOL_SHORT_NAME         "rIP"
#define TOOL_VERSION            "3.1"
#define TOOL_DESCRIPTION        "A simple and easy-to-use icons packer and extractor"
#define TOOL_DESCRIPTION_BREAK  "A simple and easy-to-use\nicons packer and extractor"
#define TOOL_RELEASE_DATE       "Apr.2024"
#define TOOL_LOGO_COLOR         0xffc800ff

#include "raylib.h"

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                         // Required for: IMGUI controls

#undef RAYGUI_IMPLEMENTATION                // Avoid including raygui implementation again

#define GUI_MAIN_TOOLBAR_IMPLEMENTATION
#include "gui_main_toolbar.h"               // GUI: Main toolbar

#define GUI_WINDOW_HELP_IMPLEMENTATION
#include "gui_window_help.h"                // GUI: Help Window

#define GUI_WINDOW_ABOUT_IMPLEMENTATION
#include "gui_window_about.h"               // GUI: About Window

#define GUI_FILE_DIALOGS_IMPLEMENTATION
#include "gui_file_dialogs.h"               // GUI: File Dialogs

// raygui embedded styles
// NOTE: Included in the same order as selector
#define MAX_GUI_STYLES_AVAILABLE   9
#include "styles/style_dark.h"              // raygui style: dark
#include "styles/style_jungle.h"            // raygui style: jungle
#include "styles/style_candy.h"             // raygui style: candy
#include "styles/style_lavanda.h"           // raygui style: lavanda
#include "styles/style_cyber.h"             // raygui style: cyber
#include "styles/style_terminal.h"          // raygui style: terminal
#include "styles/style_ashes.h"             // raygui style: ashes
#include "styles/style_bluish.h"            // raygui style: bluish

#define RPNG_IMPLEMENTATION
#include "external/rpng.h"                  // PNG chunks management

#include "external/miniz.h"                 // ZIP packaging functions definition
#include "external/miniz.c"                 // ZIP packaging implementation

// Standard C libraries
#include <stdio.h>                          // Required for: fopen(), fclose(), fread()...
#include <stdlib.h>                         // Required for: calloc(), free()
#include <string.h>                         // Required for: strcmp(), strlen()
#include <math.h>                           // Required for: ceil()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#if (!defined(_DEBUG) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
// WARNING: Comment if LOG() output is required for this tool
bool __stdcall FreeConsole(void);           // Close console from code (kernel32.lib)
#endif

// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO) && defined(_DEBUG)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

#define MAX_ICON_BUCKET_SIZE    64          // Maximum icon image entries in the bucket
#define MAX_PACK_ELEMENTS       12          // Maximum elements in pack

#define MAX_IMAGE_TEXT_SIZE     48          // Maximum image text size for text poem lines

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// NOTE: All image data referenced by entries in the image directory proceed directly after the image directory
// It is customary practice to store them in the same order as defined in the image directory

// One image entry for ico
typedef struct {
    int size;                   // Icon size (squared)
    int valid;                  // Icon valid image generated/loaded
    Image image;                // Icon image
    char text[MAX_IMAGE_TEXT_SIZE]; // Text to be embedded in the image
    bool generated;             // Image generated
} IconEntry;

// Icon bucket (platform-independant, image pool)
// NOTE: All loaded icons go into the bucket before
// being copied into platform icon pack
typedef struct {
    IconEntry *entries;         // Bucket entries
    unsigned int count;         // Bucket entries count
    unsigned int capacity;      // Bucket entries capacity
} IconBucket;

// Icon pack (platform specific)
typedef struct {
    IconEntry entries[MAX_PACK_ELEMENTS];   // Pack entries (fixed capacity)
    Texture2D textures[MAX_PACK_ELEMENTS];  // Pack textures
    unsigned int count;                     // Pack entries count, only used ones by platform!
} IconPack;

// Icon platform type
typedef enum {
    ICON_PLATFORM_WINDOWS = 0,
    ICON_PLATFORM_MACOS,
    ICON_PLATFORM_FAVICON,
    ICON_PLATFORM_ANDROID,
    ICON_PLATFORM_IOS7,
} IconPlatform;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static const char *toolName = TOOL_NAME;
static const char *toolVersion = TOOL_VERSION;
static const char *toolDescription = TOOL_DESCRIPTION;

// NOTE: Default icon sizes by platform: http://iconhandbook.co.uk/reference/chart/
static unsigned int icoSizesWindows[8] = { 256, 128, 96, 64, 48, 32, 24, 16 };              // Windows app icons
static unsigned int icoSizesMacOS[8] = { 1024, 512, 256, 128, 64, 48, 32, 16 };             // macOS app icons (16x16 not displayed for .app)
static unsigned int icoSizesFavicon[10] = { 228, 152, 144, 120, 96, 72, 64, 32, 24, 16 };   // favicon for multiple devices
static unsigned int icoSizesAndroid[10] = { 192, 144, 96, 72, 64, 48, 36, 32, 24, 16 };     // Android Launcher/Action/Dialog/Others icons, missing: 512
static unsigned int icoSizesiOS[9] = { 180, 152, 120, 87, 80, 76, 58, 40, 29 };             // iOS App/Settings/Others icons, missing: 512, 1024

// NOTE: Max length depends on OS, in Windows MAX_PATH = 256
static char inFileName[512] = { 0 };        // Input file name (required in case of drag & drop over executable)
static char outFileName[512] = { 0 };       // Output file name (required for file save/export)

static IconBucket bucket = { 0 };           // Icon image bucket

static IconPack currentPack = { 0 };
//static int platform = ICON_PLATFORM_WINDOWS;
static int *platformSizes = icoSizesWindows;
static int platformSizeCount = 8;
static int packValidCount = 0;              // Valid ico entries counter

static int sizeListActive = 0;              // Current list text entry

// WARNING: This global is required by export functions
static bool exportTextChunkChecked = true;  // Flag to embed text as a PNG chunk (rIPt)

static RenderTexture screenTarget = { 0 };

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#if defined(PLATFORM_DESKTOP) || defined(COMMAND_LINE_ONLY)
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

static void AddIconToBucket(IconBucket *bucket, const char *fileName);      // Add icon images from input file to bucket
static void RemoveIconFromBucket(IconBucket *bucket, unsigned int size);    // TODO: Remove icon from bucket -NOT USED-
static void UpdateIconPackFromBucket(IconPack *pack, IconBucket bucket);    // Update icon pack with icon bucket data
static void ClearIconBucket(IconBucket *bucket);                            // Clear icon bucket, unload all contained images

static void ResetIconPack(IconPack *pack, int platform);    // Reset icon pack, unload generated images and textures
static char *GetTextIconSizes(IconPack pack);               // Get sizes as a text array separated by semicolon (ready for GuiListView())

// Load/Save/Export data functions
static IconEntry *LoadIconPackFromICO(const char *fileName, int *count);                    // Load icon pack from .ico file
static void SaveIconPackToICO(IconEntry *entries, int entryCount, const char *fileName);    // Save icon pack to.ico file
static void ExportIconPackImages(IconEntry *entries, int entryCount, const char *fileName); // Export icon pack to multiple .png images
static IconEntry *LoadIconPackFromICNS(const char *fileName, int *count);                   // Load icon pack from .icns file
static void SaveIconPackToICNS(IconEntry *entries, int entryCount, const char *fileName);   // Save icon pack to .icns file

// Misc functions
static unsigned int CountIconPackTextLines(IconPack pack);  // Count text lines available on icon pack

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    bucket.entries = (IconEntry *)RL_CALLOC(MAX_ICON_BUCKET_SIZE, sizeof(IconEntry));
    bucket.capacity = MAX_ICON_BUCKET_SIZE;

    // Initialize current icon pack
    currentPack.count = platformSizeCount;
    for (int i = 0; i < currentPack.count; i++) currentPack.entries[i].size = platformSizes[i];

#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messsages
#endif
#if defined(COMMAND_LINE_ONLY)
    ProcessCommandLine(argc, argv);
#else
#if defined(PLATFORM_DESKTOP)
    // Command-line usage mode
    //--------------------------------------------------------------------------------------
    if (argc > 1)
    {
        if ((argc == 2) &&
            (strcmp(argv[1], "-h") != 0) &&
            (strcmp(argv[1], "--help") != 0))       // One argument (file dropped over executable?)
        {
            if (IsFileExtension(argv[1], ".ico") ||
                IsFileExtension(argv[1], ".png;.bmp;.qoi"))
            {
                strcpy(inFileName, argv[1]);        // Read input filename to open with gui interface
            }
        }
        else
        {
            ProcessCommandLine(argc, argv);
            return 0;
        }
    }
#endif
#if (!defined(_DEBUG) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
    // WARNING (Windows): If program is compiled as Window application (instead of console),
    // no console is available to show output info... solution is compiling a console application
    // and closing console (FreeConsole()) when changing to GUI interface
    // WARNING: Comment in case LOG() output is required for this tool
    FreeConsole();
#endif

    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 400;
    const int screenHeight = 380;

    InitWindow(screenWidth, screenHeight, TextFormat("%s v%s", toolName, toolVersion));
    SetExitKey(0);

    // GUI: Main Layout
    //-----------------------------------------------------------------------------------
    Vector2 anchorMain = { 0, 0 };

    int scaleAlgorythmActive = 1;

    bool btnGenIconImagePressed = false;
    bool btnClearIconImagePressed = false;

    bool iconTextEditMode = false;
    bool screenSizeActive = false;

    screenTarget = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    SetTextureFilter(screenTarget.texture, TEXTURE_FILTER_BILINEAR);
    //-----------------------------------------------------------------------------------

    // GUI: Main toolbar panel (file and visualization)
    //-----------------------------------------------------------------------------------
    GuiMainToolbarState mainToolbarState = InitGuiMainToolbar();
    //-----------------------------------------------------------------------------------

    // GUI: Help Window
    //-----------------------------------------------------------------------------------
    GuiWindowHelpState windowHelpState = InitGuiWindowHelp();
    //-----------------------------------------------------------------------------------

    // GUI: About Window
    //-----------------------------------------------------------------------------------
    GuiWindowAboutState windowAboutState = InitGuiWindowAbout();
    //-----------------------------------------------------------------------------------

    // GUI: Issue Report Window
    //-----------------------------------------------------------------------------------
    bool showIssueReportWindow = false;
    //-----------------------------------------------------------------------------------

    // GUI: Export Window
    //-----------------------------------------------------------------------------------
    bool showExportWindow = false;
    //-----------------------------------------------------------------------------------

    // GUI: Exit Window
    //-----------------------------------------------------------------------------------
    bool closeWindow = false;
    bool showExitWindow = false;
    //-----------------------------------------------------------------------------------

    // GUI: Custom file dialogs
    //-----------------------------------------------------------------------------------
    bool showLoadFileDialog = false;
    bool showExportFileDialog = false;
    //bool showExportImageDialog = false;

    bool showIconPoemWindow = false;
    //-----------------------------------------------------------------------------------

    // Check if an icon input file has been provided on command line
    if (inFileName[0] != '\0')
    {
        AddIconToBucket(&bucket, inFileName);

        // Update current pack with bucket data
        UpdateIconPackFromBucket(&currentPack, bucket);
    }

    SetTargetFPS(60);       // Set our game frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!closeWindow)    // Program must finish
    {
        // WARNING: ASINCIFY requires this line,
        // it contains the call to emscripten_sleep() for PLATFORM_WEB
        if (WindowShouldClose()) showExitWindow = true;

        // Dropped files logic
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();

            // Support gui styles
            if ((droppedFiles.count == 1) && IsFileExtension(droppedFiles.paths[0], ".rgs")) GuiLoadStyle(droppedFiles.paths[0]);

            for (int i = 0; i < droppedFiles.count; i++)
            {
                if (IsFileExtension(droppedFiles.paths[i], ".ico;.icns") ||
                    IsFileExtension(droppedFiles.paths[i], ".png;.bmp;.qoi"))
                {
                    AddIconToBucket(&bucket, droppedFiles.paths[i]);

                    // Update current pack with bucket data
                    UpdateIconPackFromBucket(&currentPack, bucket);
                }
            }

            UnloadDroppedFiles(droppedFiles);    // Unload filepaths from memory
        }
        //----------------------------------------------------------------------------------

        // Keyboard shortcuts
        //----------------------------------------------------------------------------------
        // New style file, previous in/out files registeres are reseted
        if ((IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) || mainToolbarState.btnNewFilePressed)
        {
            ClearIconBucket(&bucket);

            // Set icon pack to current platform
            ResetIconPack(&currentPack, mainToolbarState.platformActive);
        }

        // Show dialog: load input file (.ico, .icns, .png, .bmp, .qoi)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) showLoadFileDialog = true;

        // Show dialog: save icon file (.ico, .icns)
        if ((IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E)) || mainToolbarState.btnExportFilePressed)
        {
            if (packValidCount > 0)
            {
                memset(outFileName, 0, 512);

                if (mainToolbarState.platformActive == ICON_PLATFORM_MACOS)
                {
                    exportFormatActive = 2;
                    strcpy(outFileName, "icon.icns\0");
                }
                else
                {
                    exportFormatActive = 0;
                    strcpy(outFileName, "icon.ico\0");
                }

                showExportWindow = true;
            }
        }

        // Show dialog: export icon data
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
        {
            memset(outFileName, 0, 512);

            if (mainToolbarState.platformActive == ICON_PLATFORM_MACOS)
            {
                exportFormatActive = 1; // macOS icon (.icns)
                strcpy(outFileName, "icon.icns");
            }
            else
            {
                exportFormatActive = 0; // Icon (.ico)
                strcpy(outFileName, "icon.ico");
            }

            exportTextChunkChecked = true;
            showExportFileDialog = true;
        }

        // Show window: icon poem
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_SPACE) && (CountIconPackTextLines(currentPack) > 0))
        {
            showIconPoemWindow = !showIconPoemWindow;
        }

#if defined(PLATFORM_DESKTOP)
        // Toggle screen size (x2) mode
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_F)) screenSizeActive = !screenSizeActive;
#endif
        // Toggle window: help
        if (IsKeyPressed(KEY_F1)) windowHelpState.windowActive = !windowHelpState.windowActive;

        // Toggle window: about
        if (IsKeyPressed(KEY_F2)) windowAboutState.windowActive = !windowAboutState.windowActive;

        // Toggle window: issue report
        if (IsKeyPressed(KEY_F3)) showIssueReportWindow = !showIssueReportWindow;

        // Delete selected icon from list
        if ((IsKeyPressed(KEY_DELETE) && !iconTextEditMode) || btnClearIconImagePressed)
        {
            if (sizeListActive == 0)
            {
                // Reset icon pack to current platform
                ResetIconPack(&currentPack, mainToolbarState.platformActive);
            }
            else
            {
                // Reset one pack entry
                currentPack.entries[sizeListActive - 1].valid = false;
                currentPack.entries[sizeListActive - 1].image = (Image){ 0 };
                UnloadTexture(currentPack.textures[sizeListActive - 1]);
                memset(currentPack.entries[sizeListActive - 1].text, 0, MAX_IMAGE_TEXT_SIZE);
            }
        }

        // Generate icon
        if (IsKeyPressed(KEY_SPACE))
        {
            // Force icon regeneration if possible
            if (packValidCount > 0) btnGenIconImagePressed = true;
        }

        // Show closing window on ESC
        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (windowHelpState.windowActive) windowHelpState.windowActive = false;
            else if (windowAboutState.windowActive) windowAboutState.windowActive = false;
            else if (showIssueReportWindow) showIssueReportWindow = false;
            else if (showIconPoemWindow) showIconPoemWindow = false;
            else if (showExportWindow) showExportWindow = false;
        #if defined(PLATFORM_DESKTOP)
            else showExitWindow = !showExitWindow;
        #else
            else if (showLoadFileDialog) showLoadFileDialog = false;
            else if (showExportFileDialog) showExportFileDialog = false;
        #endif
        }

        // Change current style template
        //if (IsKeyPressed(KEY_RIGHT)) mainToolbarState.btnStylePressed = true;
        //----------------------------------------------------------------------------------

        // Main toolbar logic
        //----------------------------------------------------------------------------------
        // File options logic
        if (mainToolbarState.btnLoadFilePressed) showLoadFileDialog = true;
        else if (mainToolbarState.btnSaveFilePressed)
        {
            memset(outFileName, 0, 512);

            if (mainToolbarState.platformActive == ICON_PLATFORM_MACOS)
            {
                exportFormatActive = 1; // macOS icon (.icns)
                strcpy(outFileName, "icon.icns");
            }
            else
            {
                exportFormatActive = 0; // Icon (.ico)
                strcpy(outFileName, "icon.ico");
            }

            exportTextChunkChecked = true;
            showExportFileDialog = true;
        }

        // Visual options logic
        if (mainToolbarState.btnStylePressed)
        {
            mainToolbarState.visualStyleActive++;
            if (mainToolbarState.visualStyleActive > 8) mainToolbarState.visualStyleActive = 0;

            // Reset to default internal style
            // NOTE: Required to unload any previously loaded font texture
            GuiLoadStyleDefault();

            switch (mainToolbarState.visualStyleActive)
            {
                case 1: GuiLoadStyleDark(); break;
                case 2: GuiLoadStyleJungle(); break;
                case 3: GuiLoadStyleCandy(); break;
                case 4: GuiLoadStyleLavanda(); break;
                case 5: GuiLoadStyleCyber(); break;
                case 6: GuiLoadStyleTerminal(); break;
                case 7: GuiLoadStyleAshes(); break;
                case 8: GuiLoadStyleBluish(); break;
                default: break;
            }
        }

        // Help options logic
        if (mainToolbarState.btnHelpPressed) windowHelpState.windowActive = true;
        if (mainToolbarState.btnAboutPressed) windowAboutState.windowActive = true;
        if (mainToolbarState.btnIssuePressed) showIssueReportWindow = true;
        //----------------------------------------------------------------------------------

        // Basic program flow logic
        //----------------------------------------------------------------------------------
        // Calculate valid entries
        packValidCount = 0;
        for (int i = 0; i < currentPack.count; i++) if (currentPack.entries[i].valid) packValidCount++;

        // Generate new icon image, using inmmediately bigger available image in the pack
        if ((IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) || btnGenIconImagePressed)
        {
            if (sizeListActive == 0)
            {
                // Get bigger available input image in bucket
                int biggerSizeIndex = 0;
                int biggerSize = bucket.entries[0].size;
                for (int i = 1; i < bucket.count; i++)
                {
                    if (bucket.entries[i].size > biggerSize)
                    {
                        biggerSize = bucket.entries[i].size;
                        biggerSizeIndex = i;
                    }
                }

                // Generate all missing entries in the series
                for (int i = 0; i < currentPack.count; i++)
                {
                    if (!currentPack.entries[i].valid)
                    {
                        if (currentPack.entries[i].generated) UnloadImage(currentPack.entries[i].image);
                        else currentPack.entries[i].image = (Image){ 0 };   // Unlink from bucket image
                        //currentPack.entries[i].image = ImageCopy(bucket.entries[biggerSizeIndex].image);

                        Image newImage = ImageCopy(bucket.entries[biggerSizeIndex].image);

                        if (scaleAlgorythmActive == 0) ImageResizeNN(&newImage, currentPack.entries[i].size, currentPack.entries[i].size);
                        else if (scaleAlgorythmActive == 1) ImageResize(&newImage, currentPack.entries[i].size, currentPack.entries[i].size);

                        currentPack.entries[i].image = newImage;

                        UnloadTexture(currentPack.textures[i]);
                        currentPack.textures[i] = LoadTextureFromImage(currentPack.entries[i].image);

                        currentPack.entries[i].generated = true;
                        currentPack.entries[i].valid = true;
                    }
                }
            }
            else
            {
                // Get inmmediately bigger available image in the pack
                int biggerSizeIndex = 0;
                int biggerSize = 0;
                for (int i = currentPack.count; i >= 0; i--)
                {
                    if (currentPack.entries[i].valid)
                    {
                        biggerSize = currentPack.entries[i].size;

                        if (biggerSize > currentPack.entries[sizeListActive - 1].size)
                        {
                            biggerSizeIndex = i;
                            break;
                        }
                    }
                }

                // Generate only selected missing size
                if (!currentPack.entries[sizeListActive - 1].valid)
                {
                    if (currentPack.entries[sizeListActive - 1].generated) UnloadImage(currentPack.entries[sizeListActive - 1].image);
                    else currentPack.entries[sizeListActive - 1].image = (Image){ 0 };   // Unlink from bucket image
                    //currentPack.entries[sizeListActive - 1].image = ImageCopy(bucket.entries[biggerSizeIndex].image);

                    Image newImage = ImageCopy(currentPack.entries[biggerSizeIndex].image);

                    if (scaleAlgorythmActive == 0) ImageResizeNN(&newImage, currentPack.entries[sizeListActive - 1].size, currentPack.entries[sizeListActive - 1].size);
                    else if (scaleAlgorythmActive == 1) ImageResize(&newImage, currentPack.entries[sizeListActive - 1].size, currentPack.entries[sizeListActive - 1].size);

                    currentPack.entries[sizeListActive - 1].image = newImage;

                    UnloadTexture(currentPack.textures[sizeListActive - 1]);
                    currentPack.textures[sizeListActive - 1] = LoadTextureFromImage(currentPack.entries[sizeListActive - 1].image);

                    currentPack.entries[sizeListActive - 1].generated = true;
                    currentPack.entries[sizeListActive - 1].valid = true;
                }
            }
        }

        // Change active platform icons pack
        if (mainToolbarState.platformActive != mainToolbarState.prevPlatformActive)
        {
            // Reset icon pack to current platform
            ResetIconPack(&currentPack, mainToolbarState.platformActive);

            // Update current platform with icons from bucket
            UpdateIconPackFromBucket(&currentPack, bucket);

            mainToolbarState.prevPlatformActive = mainToolbarState.platformActive;
        }
        //----------------------------------------------------------------------------------

        // Screen scale logic (x2)
        //----------------------------------------------------------------------------------
        if (screenSizeActive)
        {
            // Screen size x2
            if (GetScreenWidth() < screenWidth*2)
            {
                SetWindowSize(screenWidth*2, screenHeight*2);
                SetMouseScale(0.5f, 0.5f);
            }
        }
        else
        {
            // Screen size x1
            if (screenWidth*2 >= GetScreenWidth())
            {
                SetWindowSize(screenWidth, screenHeight);
                SetMouseScale(1.0f, 1.0f);
            }
        }
        //----------------------------------------------------------------------------------

        // WARNING: Some windows should lock the main screen controls when shown
        if (windowHelpState.windowActive ||
            windowAboutState.windowActive ||
            showIssueReportWindow ||
            showIconPoemWindow ||
            showExitWindow ||
            showExportWindow ||
            showLoadFileDialog ||
            showExportFileDialog) GuiLock();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginTextureMode(screenTarget);

            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            // GUI: Main Layout: List view and icons viewer panel
            //--------------------------------------------------------------------------------------------------------------
            GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 24);
            GuiListView((Rectangle){ anchorMain.x + 10, anchorMain.y + 52, 115, 290 }, GetTextIconSizes(currentPack), NULL, &sizeListActive);
            if (sizeListActive < 0) sizeListActive = 0;

            GuiDummyRec((Rectangle){ anchorMain.x + 135, anchorMain.y + 52, 256, 256 }, NULL);
            DrawRectangleLinesEx((Rectangle){ anchorMain.x + 135, anchorMain.y + 52, 256, 256 }, 1.0f, Fade(GRAY, 0.6f));

            if (sizeListActive == 0)
            {
                // macOS supports icns up to 1024x1024 and 512x512, bigger sizes are not drawn on ALL icons mode
                for (int i = ((mainToolbarState.platformActive == ICON_PLATFORM_MACOS)? 2: 0); i < currentPack.count; i++)
                {
                    if (currentPack.entries[i].valid) DrawTexture(currentPack.textures[i], (int)anchorMain.x + 135, (int)anchorMain.y + 52, WHITE);
                    else GuiPanel((Rectangle){ anchorMain.x + 135, anchorMain.y + 52, currentPack.entries[i].size, currentPack.entries[i].size }, NULL);
                }
            }
            else if (sizeListActive > 0)
            {
                if (mainToolbarState.platformActive == ICON_PLATFORM_MACOS)
                {
                    // macOS supports icns up to 1024x1024 and 512x512, those sizes require a scaled drawing
                    float scaling = 256.0f/currentPack.entries[sizeListActive - 1].size;
                    if (scaling > 1.0f) scaling = 1.0f;

                    if (currentPack.entries[sizeListActive - 1].valid)
                    {
                        DrawTextureEx(currentPack.textures[sizeListActive - 1],
                            (Vector2){ anchorMain.x + 135 + 128 - (currentPack.entries[sizeListActive - 1].size*scaling/2),
                            anchorMain.y + 52 + 128 - (currentPack.entries[sizeListActive - 1].size*scaling/2) }, 0.0f, scaling, WHITE);
                    }
                    else
                    {
                        GuiPanel((Rectangle){ anchorMain.x + 135 + 128 - currentPack.entries[sizeListActive - 1].size*scaling/2,
                            anchorMain.y + 52 + 128 - currentPack.entries[sizeListActive - 1].size*scaling/2,
                            currentPack.entries[sizeListActive - 1].size*scaling, currentPack.entries[sizeListActive - 1].size*scaling }, NULL);
                    }

                    if (scaling < 1.0f) DrawText(TextFormat("SCALE: %0.2f", scaling), (int)anchorMain.x + 135 + 10, (int)anchorMain.y + 52 + 256 - 24, 20, GREEN);
                }
                else
                {
                    if (currentPack.entries[sizeListActive - 1].valid)
                    {
                        DrawTexture(currentPack.textures[sizeListActive - 1],
                            (int)anchorMain.x + 135 + 128 - currentPack.entries[sizeListActive - 1].size/2,
                            (int)anchorMain.y + 52 + 128 - currentPack.entries[sizeListActive - 1].size/2, WHITE);
                    }
                    else
                    {
                        GuiPanel((Rectangle){ anchorMain.x + 135 + 128 - currentPack.entries[sizeListActive - 1].size/2,
                            anchorMain.y + 52 + 128 - currentPack.entries[sizeListActive - 1].size/2,
                            currentPack.entries[sizeListActive - 1].size, currentPack.entries[sizeListActive - 1].size }, NULL);
                    }
                }
            }

            // Clear/generate selected icon image level
            // NOTE: Enabled buttons depend on several circunstances
            if ((packValidCount == 0) || ((sizeListActive > 0) && !currentPack.entries[sizeListActive - 1].valid)) GuiDisable();
            btnClearIconImagePressed = GuiButton((Rectangle){ anchorMain.x + 135 + 256 - 48 - 8, anchorMain.y + 52 + 256 - 24 - 4, 24, 24 }, "#143#");
            GuiEnable();

            if ((packValidCount == 0) || ((sizeListActive > 0) && currentPack.entries[sizeListActive - 1].valid)) GuiDisable();
            btnGenIconImagePressed = GuiButton((Rectangle){ anchorMain.x + 135 + 256 - 24 - 4, anchorMain.y + 52 + 256 - 24 - 4, 24, 24 }, "#142#");
            GuiEnable();

            // Icon image text for embedding
            if ((sizeListActive == 0) || !currentPack.entries[sizeListActive - 1].valid) GuiDisable();
            if (GuiTextBox((Rectangle){ anchorMain.x + 135, anchorMain.y + 52 + 256 + 8, 256, 26 }, (sizeListActive == 0)? "Add custom image text here!" : currentPack.entries[sizeListActive - 1].text, MAX_IMAGE_TEXT_SIZE, iconTextEditMode)) iconTextEditMode = !iconTextEditMode;
            GuiEnable();
            //--------------------------------------------------------------------------------------------------------------

            // GUI: Main toolbar panel
            //----------------------------------------------------------------------------------
            GuiMainToolbar(&mainToolbarState);
            //----------------------------------------------------------------------------------

            // GUI: Status bar
            //----------------------------------------------------------------------------------------
            GuiSetStyle(STATUSBAR, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
            GuiStatusBar((Rectangle){ anchorMain.x + 0, screenHeight - 24, 136, 24 }, TextFormat("BUCKET COUNT: %i", bucket.count));
            GuiStatusBar((Rectangle){ anchorMain.x + 136 - 1, screenHeight - 24, 120, 24 }, TextFormat("PACK COUNT: %i", currentPack.count));
            GuiStatusBar((Rectangle){ anchorMain.x + 256 - 2, screenHeight - 24, screenWidth - 252 - 2, 24 }, (sizeListActive > 0)? TextFormat("ICON TEXT: %i/%i", strlen(currentPack.entries[sizeListActive - 1].text), MAX_IMAGE_TEXT_SIZE - 1) : NULL);
            GuiSetStyle(STATUSBAR, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
            //----------------------------------------------------------------------------------------

            // NOTE: If some overlap window is open and main window is locked, we draw a background rectangle, probably too big in 2x mode
            if (GuiIsLocked()) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.85f));

            // WARNING: Before drawing the windows, we unlock them
            GuiUnlock();

            // GUI: Icon poem Window
            //----------------------------------------------------------------------------------------
            if (showIconPoemWindow)
            {
                unsigned int textLinesCount = CountIconPackTextLines(currentPack);

                if (textLinesCount > 0)
                {
                    Vector2 windowIconPoemOffset = (Vector2){ (float)screenWidth/2 - 320/2, (float)screenHeight/2 - (88 + 50 + textLinesCount*20)/2 };
                    showIconPoemWindow = !GuiWindowBox((Rectangle){ windowIconPoemOffset.x, windowIconPoemOffset.y, 320, 24 + 12 + textLinesCount*24 + 12 + 28 + 12 }, "#10#Icon poem found!");

                    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                    for (int i = 0, k = 0; i < currentPack.count; i++)
                    {
                        if (currentPack.entries[i].valid && (currentPack.entries[i].text[0] != '\0'))
                        {
                            GuiLabel((Rectangle){ windowIconPoemOffset.x + 12, windowIconPoemOffset.y + 24 + 12 + 24*k, 320 - 24, 24 }, TextFormat("%s", currentPack.entries[i].text));
                            k++;
                        }
                    }
                    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);

                    if (GuiButton((Rectangle){ windowIconPoemOffset.x + 10, windowIconPoemOffset.y + 24 + 12 + textLinesCount*24 + 12, 320 - 24, 28 }, "#186#I love it!"))
                    {
                        showIconPoemWindow = false;
                    }
                }
                else showIconPoemWindow = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Help Window
            //----------------------------------------------------------------------------------------
            windowHelpState.windowBounds.x = (float)screenWidth/2 - windowHelpState.windowBounds.width/2;
            windowHelpState.windowBounds.y = (float)screenHeight/2 - windowHelpState.windowBounds.height/2;
            GuiWindowHelp(&windowHelpState);
            //----------------------------------------------------------------------------------------

            // GUI: About Window
            //----------------------------------------------------------------------------------------
            windowAboutState.windowBounds.x = (float)screenWidth/2 - windowAboutState.windowBounds.width/2;
            windowAboutState.windowBounds.y = (float)screenHeight/2 - windowAboutState.windowBounds.height/2;
            GuiWindowAbout(&windowAboutState);
            //----------------------------------------------------------------------------------------

            // GUI: Issue Report Window
            //----------------------------------------------------------------------------------------
            if (showIssueReportWindow)
            {
                Rectangle messageBox = { (float)screenWidth/2 - 300/2, (float)screenHeight/2 - 190/2 - 20, 300, 190 };
                int result = GuiMessageBox(messageBox, "#220#Report Issue",
                    "Do you want to report any issue or\nfeature request for this program?\n\ngithub.com/raysan5/riconpacker", "#186#Report on GitHub");

                if (result == 1)    // Report issue pressed
                {
                    OpenURL("https://github.com/raysan5/riconpacker/issues");
                    showIssueReportWindow = false;
                }
                else if (result == 0) showIssueReportWindow = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export Window
            //----------------------------------------------------------------------------------------
            if (showExportWindow)
            {
                Rectangle messageBox = { (float)screenWidth/2 - 248/2, (float)screenHeight/2 - 200/2, 248, 112 };
                int result = GuiMessageBox(messageBox, "#7#Export Icon File", " ", "#7#Export Icon");

                GuiLabel((Rectangle){ messageBox.x + 12, messageBox.y + 12 + 24, 106, 24 }, "Icon Format:");

                // NOTE: If current platform is macOS, we support .icns file export
                GuiComboBox((Rectangle){ messageBox.x + 12 + 88, messageBox.y + 12 + 24, 136, 24 }, (mainToolbarState.platformActive == 1)? "Icon (.ico);Images (.png);Icns (.icns)" : "Icon (.ico);Images (.png)", &exportFormatActive);

                // WARNING: exportTextChunkChecked is used as a global variable required by SaveICO() and SaveICNS() functions
                //GuiCheckBox((Rectangle){ messageBox.x + 20, messageBox.y + 48 + 24, 16, 16 }, "Export text poem with icon", &exportTextChunkChecked);

                if (result == 1)    // Export button pressed
                {
                    showExportWindow = false;
                    showExportFileDialog = true;
                }
                else if (result == 0) showExportWindow = false;
            }
            //----------------------------------------------------------------------------------

            // GUI: Exit Window
            //----------------------------------------------------------------------------------------
            if (showExitWindow)
            {
                int result = GuiMessageBox((Rectangle){ screenWidth/2.0f - 125, screenHeight/2.0f - 50, 250, 100 }, "#159#Closing rIconPacker", "Do you really want to exit?", "Yes;No");

                if ((result == 0) || (result == 2)) showExitWindow = false;
                else if (result == 1) closeWindow = true;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Load File Dialog (and loading logic)
            //----------------------------------------------------------------------------------------
            if (showLoadFileDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Load icon or image file", inFileName, "Ok", "Just drag and drop your file!");
#else
                int result = GuiFileDialog(DIALOG_OPEN_FILE, "Load icon or image file...", inFileName, "*.ico;*.icns;*.png;*.bmp;*.qoi", "Icon or Image Files");
#endif
                if (result == 1)
                {
                    AddIconToBucket(&bucket, inFileName);   // Load icon file

                    // Update current pack with bucket data
                    UpdateIconPackFromBucket(&currentPack, bucket);
                }

                if (result >= 0) showLoadFileDialog = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export File Dialog (and saving logic)
            //----------------------------------------------------------------------------------------
            if (showExportFileDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = -1;
                if (exportFormatActive == 0) result = GuiTextInputBox((Rectangle){ screenWidth/2 - 280/2, screenHeight/2 - 112/2 - 30, 280, 112 }, "#7#Export icon file...", NULL, "#7#Export", outFileName, 512, NULL);
                else if (exportFormatActive == 1) result = GuiTextInputBox((Rectangle){ screenWidth/2 - 280/2, screenHeight/2 - 112/2 - 30, 280, 112 }, "#7#Export image files...", NULL, "#7#Export", outFileName, 512, NULL);
                else if (exportFormatActive == 2) result = GuiTextInputBox((Rectangle){ screenWidth/2 - 280/2, screenHeight/2 - 112/2 - 30, 280, 112 }, "#7#Export icns files...", NULL, "#7#Export", outFileName, 512, NULL);
#else
                int result = -1;
                if (exportFormatActive == 0) result = GuiFileDialog(DIALOG_SAVE_FILE, "Export icon file...", outFileName, "*.ico", "Icon File (*.ico)");
                else if (exportFormatActive == 1) result = GuiFileDialog(DIALOG_SAVE_FILE, "Export image files...", outFileName, "*.png", "Image Files (*.png)");
                else if (exportFormatActive == 2) result = GuiFileDialog(DIALOG_SAVE_FILE, "Export icns file...", outFileName, "*.icns", "Icns File (*.icns)");
#endif
                if (result == 1)
                {
                    // Check for valid extension and make sure it is
                    if (GetFileExtension(outFileName) == NULL)
                    {
                        if ((exportFormatActive == 0) && !IsFileExtension(outFileName, ".ico")) strcat(outFileName, ".ico\0");
                        else if ((exportFormatActive == 1) && !IsFileExtension(outFileName, ".png")) strcat(outFileName, ".png\0");
                        else if ((exportFormatActive == 2) && !IsFileExtension(outFileName, ".icns")) strcat(outFileName, ".icns\0");
                    }

                    // Save into icon file provided pack entries
                    if (exportFormatActive == 0) SaveIconPackToICO(currentPack.entries, currentPack.count, outFileName);
                    else if (exportFormatActive == 1) ExportIconPackImages(currentPack.entries, currentPack.count, outFileName);
                    else if (exportFormatActive == 2) SaveIconPackToICNS(currentPack.entries, currentPack.count, outFileName);

                    /*
                    // Testing packaging exported icons into a .zip file -> WORKS
                    if (exportFormatActive == 1)
                    {
                        // Package all created image files (in browser File-System) into a .zip to be exported
                        mz_zip_archive zip = { 0 };
                        mz_bool mz_ret = mz_zip_writer_init_file(&zip, TextFormat("%s.zip", outFileName), 0);
                        if (!mz_ret) printf("Could not initialize zip archive\n");

                        for (int i = 0; i < currentPack.count; i++)
                        {
                            if (currentPack.entries[i].valid)
                            {
                                mz_ret = mz_zip_writer_add_file(&zip,
                                    TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), currentPack.entries[i].image.width, currentPack.entries[i].image.height),
                                    TextFormat("%s/%s_%ix%i.png", GetDirectoryPath(outFileName), GetFileNameWithoutExt(outFileName), currentPack.entries[i].image.width, currentPack.entries[i].image.height),
                                    "Comment", (mz_uint16)strlen("Comment"), MZ_BEST_SPEED);
                                if (!mz_ret) printf("Could not add file to zip archive\n");
                            }
                        }

                        mz_ret = mz_zip_writer_finalize_archive(&zip);
                        if (!mz_ret) printf("Could not finalize zip archive\n");

                        mz_ret = mz_zip_writer_end(&zip);
                        if (!mz_ret) printf("Could not finalize zip writer\n");
                    }
                    */
                #if defined(PLATFORM_WEB)
                    if (exportFormatActive == 1)
                    {
                        // Package all created image files (in browser File-System) into a .zip to be exported
                        mz_zip_archive zip = { 0 };
                        mz_bool mz_ret = mz_zip_writer_init_file(&zip, TextFormat("%s.zip", outFileName), 0);
                        if (!mz_ret) printf("Could not initialize zip archive\n");

                        for (int i = 0; i < currentPack.count; i++)
                        {
                            if (currentPack.entries[i].valid)
                            {
                                mz_ret = mz_zip_writer_add_file(&zip,
                                    TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), currentPack.entries[i].image.width, currentPack.entries[i].image.height),
                                    TextFormat("%s\\%s_%ix%i.png", GetDirectoryPath(outFileName), GetFileNameWithoutExt(outFileName), currentPack.entries[i].image.width, currentPack.entries[i].image.height),
                                    "Comment", (mz_uint16)strlen("Comment"), MZ_BEST_SPEED);
                                if (!mz_ret) printf("Could not add file to zip archive\n");
                            }
                        }

                        mz_ret = mz_zip_writer_finalize_archive(&zip);
                        if (!mz_ret) printf("Could not finalize zip archive\n");

                        mz_ret = mz_zip_writer_end(&zip);
                        if (!mz_ret) printf("Could not finalize zip writer\n");

                        char tempFileName[512] = { 0 };
                        strcpy(tempFileName, TextFormat("%s.zip", outFileName));
                        emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", tempFileName, GetFileName(tempFileName)));
                    }
                    else
                    {
                        // Download file from MEMFS (emscripten memory filesystem)
                        // NOTE: Second argument must be a simple filename (we can't use directories)
                        // NOTE: Included security check to (partially) avoid malicious code on PLATFORM_WEB
                        if (strchr(outFileName, '\'') == NULL) emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", outFileName, GetFileName(outFileName)));
                    }
                #endif
                }

                if (result >= 0) showExportFileDialog = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export Image Dialog (and saving logic)
            //----------------------------------------------------------------------------------------
            /*
            if (showExportImageDialog)
            {
                strcpy(outFileName, TextFormat("icon_%ix%i.png", currentPack.icons[sizeListActive - 1].image.width, currentPack.icons[sizeListActive - 1].image.height));

#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_TEXTINPUT, "Export image file...", outFileName, "Ok;Cancel", NULL);
#else
                int result = GuiFileDialog(DIALOG_SAVE_FILE, "Export image file...", outFileName, "*.png", "Image File (*.png)");
#endif
                if (result == 1)
                {
                    // Check for valid extension and make sure it is
                    if ((GetFileExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".png")) strcat(outFileName, ".png\0");

                    ExportImage(currentPack.icons[sizeListActive - 1].image, outFileName);

                #if defined(PLATFORM_WEB)
                    // Download file from MEMFS (emscripten memory filesystem)
                    // NOTE: Second argument must be a simple filename (we can't use directories)
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", outFileName, GetFileName(outFileName)));
                #endif
                }

                if (result >= 0) showExportImageDialog = false;
            }
            */
            //----------------------------------------------------------------------------------------

        EndTextureMode();

        BeginDrawing();
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            // Draw screen scaled if required
            if (screenSizeActive) DrawTexturePro(screenTarget.texture, (Rectangle){ 0, 0, (float)screenTarget.texture.width, -(float)screenTarget.texture.height }, (Rectangle){ 0, 0, (float)screenTarget.texture.width*2, (float)screenTarget.texture.height*2 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
            else DrawTextureRec(screenTarget.texture, (Rectangle){ 0, 0, (float)screenTarget.texture.width, -(float)screenTarget.texture.height }, (Vector2){ 0, 0 }, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload icon packs data
    ResetIconPack(&currentPack, 0);

    // Unload icon bucket data
    ClearIconBucket(&bucket);
    RL_FREE(bucket.entries);

    CloseWindow();      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

#endif      // !COMMAND_LINE_ONLY

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------------
#if defined(PLATFORM_DESKTOP) || defined(COMMAND_LINE_ONLY)
// Show command line usage info
static void ShowCommandLineInfo(void)
{
    printf("\n////////////////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                                        //\n");
    printf("// %s v%s - %s                 //\n", toolName, toolVersion, toolDescription);
    printf("// powered by raylib v%s and raygui v%s                                             //\n", RAYLIB_VERSION, RAYGUI_VERSION);
    printf("// more info and bugs-report: ray[at]raylibtech.com                                       //\n");
    printf("//                                                                                        //\n");
    printf("// Copyright (c) 2018-2025 raylib technologies (@raylibtech)                              //\n");
    printf("//                                                                                        //\n");
    printf("////////////////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("USAGE:\n\n");
    printf("    > riconpacker [--help] --input <file01.ext>,[file02.ext],... [--output <filename.ico>]\n");
    printf("                  [--out-sizes <size01>,[size02],...] [--out-platform <value>] [--scale-algorythm <value>]\n");
    printf("                  [--extract-size <size01>,[size02],...] [--extract-all]\n");

    printf("\nOPTIONS:\n\n");
    printf("    -h, --help                      : Show tool version and command line usage help\n\n");
    printf("    -i, --input <file01.ext>,[file02.ext],...\n");
    printf("                                    : Define input file(s). Comma separated for multiple files.\n");
    printf("                                      Supported extensions: .ico, .icns, .png, .bmp, .qoi\n\n");
    printf("    -o, --output <filename.ico>     : Define output icon file.\n");
    printf("                                      NOTE: If not specified, defaults to: output.ico\n\n");
    printf("    -op, --out-platform <value>     : Define out sizes by platform scheme.\n");
    printf("                                      Supported values:\n");
    printf("                                          0 - Windows (Sizes: 256, 128, 96, 64, 48, 32, 24, 16)\n");
    printf("                                          1 - macOS (Sizes: 1024, 512, 256, 128, 64, 48, 32, 16)\n");
    printf("                                          2 - favicon (Sizes: 228, 152, 144, 120, 96, 72, 64, 32, 24, 16)\n");
    printf("                                          3 - Android (Sizes: 192, 144, 96, 72, 64, 48, 36, 32, 24, 16)\n");
    printf("                                          4 - iOS (Sizes: 180, 152, 120, 87, 80, 76, 58, 40, 29)\n");
    printf("                                      NOTE: If not specified, any icon size can be generated\n\n");
    printf("    -os, --out-sizes <size01>,[size02],...\n");
    printf("                                    : Define output sizes for the output.\n");
    printf("                                      If output size is not provided as input, it's generated.\n");
    printf("                                      NOTE: Generated icons are always squared.\n\n");
    printf("    -sa, --scale-algorythm <value>  : Define the algorythm used to scale images.\n");
    printf("                                      Supported values:\n");
    printf("                                          1 - Nearest-neighbor scaling algorythm\n");
    printf("                                          2 - Bicubic scaling algorythm (default)\n\n");
    printf("    -xs, --extract-size <size01>,[size02],...\n");
    printf("                                    : Extract image sizes from input (if size is available)\n");
    printf("                                      NOTE: Exported images name: output_{size}.png\n\n");
    printf("    -xa, --extract-all              : Extract all images from icon.\n");
    printf("                                      NOTE: Exported images naming: output_{size}.png,...\n\n");
    printf("\nEXAMPLES:\n\n");
    printf("    > riconpacker --input image.png --output image.ico --out-platform 0\n");
    printf("        Process <image.png> to generate <image.ico> including full Windows icons sequence\n\n");
    printf("    > riconpacker --input image.png --out-sizes 256,64,48,32\n");
    printf("        Process <image.png> to generate <output.ico> including sizes: 256,64,48,32\n");
    printf("        NOTE: If a specific size is not found on input file, it's generated from bigger available size\n\n");
    printf("    > riconpacker --input image.ico --extract-all\n");
    printf("        Extract all available images contained in image.ico\n\n");
}

// Process command line input
static void ProcessCommandLine(int argc, char *argv[])
{
    #define MAX_OUTPUT_SIZES    64      // Maximum number of output sizes to generate
    #define MAX_EXTRACT_SIZES   64      // Maximum number of sizes to extract

    // CLI required variables
    bool showUsageInfo = false;         // Toggle command line usage info

    int inputFilesCount = 0;
    char **inputFiles = NULL;           // Input file names
    char outFileName[512] = { 0 };      // Output file name

    int outPlatform = 0;                // Output platform sizes scheme

    int outSizes[MAX_OUTPUT_SIZES] = { 0 }; // Sizes to generate
    int outSizesCount = 0;              // Number of sizes to generate

    int scaleAlgorythm = 2;             // Scaling algorythm on generation

    bool extractSize = false;           // Extract size required
    int extractSizes[MAX_EXTRACT_SIZES] = { 0 }; // Sizes to extract
    int extractSizesCount = 0;          // Number of sizes to extract

    bool extractAll = false;            // Extract all sizes required

#if defined(COMMAND_LINE_ONLY)
    if (argc == 1) showUsageInfo = true;
#endif

    // Process command line arguments
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showUsageInfo = true;
        }
        else if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--input") == 0))
        {
            // Check for valid argument
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                char **files = TextSplit(argv[i + 1], ',', &inputFilesCount);

                inputFiles = (char **)RL_CALLOC(inputFilesCount, sizeof(char *));
                for (int j = 0; j < inputFilesCount; j++)
                {
                    inputFiles[j] = (char *)RL_CALLOC(256, 1);    // Input file name
                    strcpy(inputFiles[j], files[j]);
                }

                i++;
            }
            else printf("WARNING: No input file(s) provided\n");
        }
        else if ((strcmp(argv[i], "-o") == 0) || (strcmp(argv[i], "--output") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                if (IsFileExtension(argv[i + 1], ".ico") ||
                    ((outPlatform == 1) && IsFileExtension(argv[i + 1], ".icns")))      // macOS
                {
                    strcpy(outFileName, argv[i + 1]);   // Read output filename
                }

                i++;
            }
            else printf("WARNING: Output file extension not recognized.\n");
        }
        else if ((strcmp(argv[i], "-os") == 0) || (strcmp(argv[i], "--out-sizes") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                int numValues = 0;
                char **values = TextSplit(argv[i + 1], ',', &numValues);

                for (int j = 0; j < numValues; j++)
                {
                    int value = TextToInteger(values[j]);

                    if ((value > 0) && (value <= 256))
                    {
                        outSizes[j] = value;
                        outSizesCount++;
                    }
                    else printf("WARNING: Provided generation size not valid: %i\n", value);
                }
            }
            else printf("WARNING: No sizes provided\n");
        }
        else if ((strcmp(argv[i], "-op") == 0) || (strcmp(argv[i], "--out-platform") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                int platform = TextToInteger(argv[i + 1]);   // Read provided platform value

                if ((platform >= 0) && (platform < 5)) outPlatform = platform;
                else printf("WARNING: Platform requested not recognized\n");
            }
            else printf("WARNING: No platform provided\n");
        }
        else if ((strcmp(argv[i], "-sa") == 0) || (strcmp(argv[i], "--scale-algorythm") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                int scale = TextToInteger(argv[i + 1]);   // Read provided scale algorythm value

                if ((scale == 1) || (scale == 2)) scaleAlgorythm = scale;
                else printf("WARNING: Scale algorythm not recognized, default to Bicubic\n");
            }
            else printf("WARNING: No scale algortyhm provided\n");
        }
        else if ((strcmp(argv[i], "-xs") == 0) || (strcmp(argv[i], "--extract-size") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                extractSize = true;

                int numValues = 0;
                char **values = TextSplit(argv[i + 1], ',', &numValues);

                for (int j = 0; j < numValues; j++)
                {
                    int value = TextToInteger(values[j]);

                    if ((value > 0) && (value <= 256))
                    {
                        extractSizes[j] = value;
                        extractSizesCount++;
                    }
                    else printf("WARNING: Requested extract size not valid: %i\n", value);
                }
            }
            else printf("WARNING: No sizes provided\n");
        }
        else if ((strcmp(argv[i], "-xa") == 0) || (strcmp(argv[i], "--extract-all") == 0)) extractAll = true;
    }

    // Process input files if provided
    if (inputFilesCount > 0)
    {
        if (outFileName[0] == '\0') strcpy(outFileName, (outPlatform == 1)? "output.icns" : "output.ico");  // Set a default name for output in case not provided

        printf("\nInput files:      %s", inputFiles[0]);
        for (int i = 1; i < inputFilesCount; i++) printf(",%s", inputFiles[i]);
        printf("\nOutput file:      %s\n\n", outFileName);

        printf(" > PROCESSING INPUT FILES\n");

        // Load input files (all of them) into bucket,
        // NOTE: If one size has been previously loaded, it is overriden
        for (int i = 0; i < inputFilesCount; i++)
        {
            AddIconToBucket(&bucket, inputFiles[i]);
            printf("\nInput file: %s - Added to icon bucket - Total files: %i\n", inputFiles[i], bucket.count);

            RL_FREE(inputFiles[i]);    // Free input file name memory
        }

        RL_FREE(inputFiles);           // Free input file names array memory

        // Get bigger available input image in bucket
        int biggerSizeIndex = 0;
        int biggerSize = bucket.entries[0].size;

        for (int i = 1; i < bucket.count; i++)
        {
            if (bucket.entries[i].size > biggerSize)
            {
                biggerSize = bucket.entries[i].size;
                biggerSizeIndex = i;
            }
        }

        printf("\nAll input images processed.\n");
        printf("Image sizes added to the bucket: %i (%i", bucket.count, bucket.entries[0].size);
        for (int i = 1; i < bucket.count; i++) printf(",%i", bucket.entries[i].size);
        printf(")\n");
        printf("Biggest size available: %i\n\n", biggerSize);

        printf(" > PROCESSING OUTPUT FILE\n\n");

        // Generate output sizes list by platform scheme
        switch (outPlatform)
        {
            case ICON_PLATFORM_WINDOWS: for (int i = 0; i < 8; i++) { outSizes[outSizesCount] = icoSizesWindows[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_MACOS: for (int i = 0; i < 8; i++) { outSizes[outSizesCount] = icoSizesMacOS[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_FAVICON: for (int i = 0; i < 10; i++) { outSizes[outSizesCount] = icoSizesFavicon[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_ANDROID: for (int i = 0; i < 10; i++) { outSizes[outSizesCount] = icoSizesAndroid[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_IOS7: for (int i = 0; i < 9; i++) { outSizes[outSizesCount] = icoSizesiOS[i]; outSizesCount++; }; break;
            default: return;
        }

        IconEntry *outPack = NULL;
        int outPackCount = 0;

        if (outSizesCount > 0)
        {
            printf("Output sizes requested: %i", outSizes[0]);
            for (int i = 1; i < outSizesCount; i++) printf(",%i", outSizes[i]);
            printf("\n");

            // Generate custom sizes if required, use biggest available input size and use provided scale algorythm
            outPackCount = outSizesCount;
            outPack = (IconEntry *)RL_CALLOC(outPackCount, sizeof(IconEntry));

            // Copy from inputPack or generate if required
            for (int i = 0; i < outPackCount; i++)
            {
                outPack[i].size = outSizes[i];

                // Check input pack for size to copy
                for (int j = 0; j < bucket.count; j++)
                {
                    if (outPack[i].size == bucket.entries[j].size)
                    {
                        printf(" > Size %i: COPIED from input images.\n", outPack[i].size);
                        outPack[i].image = bucket.entries[j].image;
                        outPack[i].valid = true;
                        break;
                    }
                }

                // Generate image size if not copied
                if (!outPack[i].valid)
                {
                    printf(" > Size %i: GENERATED from input bigger image (%i).\n", outPack[i].size, biggerSize);
                    outPack[i].image = ImageCopy(bucket.entries[biggerSizeIndex].image);

                    if (scaleAlgorythm == 1) ImageResizeNN(&outPack[i].image, outPack[i].size, outPack[i].size);
                    else if (scaleAlgorythm == 2) ImageResize(&outPack[i].image, outPack[i].size, outPack[i].size);

                    outPack[i].generated = true;
                    outPack[i].valid = true;
                }
            }

            printf("\n");

            // Save into icon file provided pack entries
            // NOTE: Only valid entries are exported, png zip packaging also done (if required)
            if (outPlatform == 1) SaveIconPackToICNS(outPack, outPackCount, outFileName);
            else SaveIconPackToICO(outPack, outPackCount, outFileName);
        }
        else printf("WARNING: No output sizes defined\n");

        // Extract required entries: all or provided sizes (only available ones)
        if (extractAll)
        {
            // Extract all input pack entries
            for (int i = 0; i < bucket.count; i++)
            {
                if (bucket.entries[i].valid)
                {
                    printf(" > Image extract requested (%i): %s_%ix%i.png\n", bucket.entries[i].size, GetFileNameWithoutExt(outFileName), bucket.entries[i].size, bucket.entries[i].size);
                    ExportImage(bucket.entries[i].image, TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), bucket.entries[i].size, bucket.entries[i].size));
                }
            }
        }
        else if (extractSize)
        {
            // Extract requested sizes from pack (if available)
            for (int i = 0; i < bucket.count; i++)
            {
                for (int j = 0; j < extractSizesCount; j++)
                {
                    if (bucket.entries[i].size == extractSizes[j])
                    {
                        printf(" > Image extract requested (%i): %s_%ix%i.png\n", extractSizes[j], GetFileNameWithoutExt(outFileName), bucket.entries[i].size, bucket.entries[i].size);
                        ExportImage(bucket.entries[i].image, TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), bucket.entries[i].size, bucket.entries[i].size));
                    }
                }
            }

            // Extract requested sizes from output pack (if available)
            for (int i = 0; i < outPackCount; i++)
            {
                for (int j = 0; j < extractSizesCount; j++)
                {
                    if ((extractSizes[j] > 0) && (outPack[i].size == extractSizes[j]))
                    {
                        printf(" > Image extract requested (%i): %s_%ix%i.png\n", extractSizes[j], GetFileNameWithoutExt(outFileName), outPack[i].size, outPack[i].size);
                        ExportImage(outPack[i].image, TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), outPack[i].size, outPack[i].size));
                    }
                }
            }
        }

        // Memory cleaning
        for (int i = 0; i < bucket.count; i++) UnloadImage(bucket.entries[i].image);
        for (int i = 0; i < outPackCount; i++) if (outPack[i].generated) UnloadImage(outPack[i].image);
        RL_FREE(outPack);
    }

    if (showUsageInfo) ShowCommandLineInfo();
}
#endif

//--------------------------------------------------------------------------------------------
// Load/Save/Export functions
//--------------------------------------------------------------------------------------------
// Get sizes as a text array separated by semicolon (ready for GuiListView())
static char *GetTextIconSizes(IconPack pack)
{
    static char buffer[512] = { 0 };
    memset(buffer, 0, 512);

    int offset = 0;
    int length = 0;
    memcpy(buffer, "ALL;", 4);
    offset += 4;

    for (int i = 0; i < pack.count; i++)
    {
        length = TextLength(TextFormat("%i x %i;", pack.entries[i].size, pack.entries[i].size));
        memcpy(buffer + offset, TextFormat("%i x %i;", pack.entries[i].size, pack.entries[i].size), length);
        offset += length;
    }

    buffer[offset - 1] = '\0';

    return buffer;
}

// Icon File Header (6 bytes)
typedef struct {
    unsigned short reserved;    // Must always be 0
    unsigned short imageType;   // Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image. Other values are invalid
    unsigned short imageCount;  // Specifies number of entries in the file
} IcoHeader;

// Icon Entry info (16 bytes)
typedef struct {
    unsigned char width;        // Specifies image width in pixels. Can be any number between 0 and 255. Value 0 means image width is 256 pixels
    unsigned char height;       // Specifies image height in pixels. Can be any number between 0 and 255. Value 0 means image height is 256 pixels
    unsigned char colpalette;   // Specifies number of colors in the color palette. Should be 0 if the image does not use a color palette
    unsigned char reserved;     // Reserved. Should be 0
    unsigned short planes;      // In ICO format: Specifies color planes; should be 0 or 1
    // In CUR format: Specifies the horizontal coordinates of the hotspot in number of pixels from the left
    unsigned short bpp;         // In ICO format: Specifies bits per pixel. [Notes 4]
    // In CUR format: Specifies the vertical coordinates of the hotspot in number of pixels from the top.
    unsigned int size;          // Specifies the size of the image's data in bytes
    unsigned int offset;        // Specifies the offset of BMP or PNG data from the beginning of the ICO/CUR file
} IcoDirEntry;

// Icon data loader
static IconEntry *LoadIconPackFromICO(const char *fileName, int *count)
{
    IconEntry *entries = NULL;
    int imageCounter = 0;

    FILE *icoFile = fopen(fileName, "rb");

    if (icoFile != NULL)
    {
        // Load .ico information
        IcoHeader icoHeader = { 0 };
        fread(&icoHeader, sizeof(IcoHeader), 1, icoFile);

        entries = (IconEntry *)RL_CALLOC(icoHeader.imageCount, sizeof(IconEntry));

        IcoDirEntry *icoDirEntry = (IcoDirEntry *)RL_CALLOC(icoHeader.imageCount, sizeof(IcoDirEntry));

        for (int i = 0; i < icoHeader.imageCount; i++) fread(&icoDirEntry[i], sizeof(IcoDirEntry), 1, icoFile);

        for (int i = 0; i < icoHeader.imageCount; i++)
        {
            unsigned char *icoImageData = (unsigned char *)RL_CALLOC(icoDirEntry[i].size, 1);
            fread(icoImageData, 1, icoDirEntry[i].size, icoFile);    // Read icon image data

            // Verify PNG signature for loaded image data
            if ((icoImageData[0] == 0x89) &&
                (icoImageData[1] == 0x50) &&
                (icoImageData[2] == 0x4e) &&
                (icoImageData[3] == 0x47) &&
                (icoImageData[4] == 0x0d) &&
                (icoImageData[5] == 0x0a) &&
                (icoImageData[6] == 0x1a) &&
                (icoImageData[7] == 0x0a))
            {
                // Reading image data from memory buffer
                // WARNING: Image data on th IcoDirEntry may be in either:
                //  - Windows BMP format, excluding the BITMAPFILEHEADER structure
                //  - PNG format, stored in its entirety
                // NOTE: We are only supporting the PNG format, not BMP data
                entries[i].image = LoadImageFromMemory(".png", icoImageData, icoDirEntry[i].size);

                if ((entries[i].image.data != NULL) && (entries[i].image.width != 0))
                {
                    entries[i].size = entries[i].image.width;   // Icon size (expected squared)
                    entries[i].valid = false;                   // Not valid until it is checked against the current package (sizes)

                    // Read custom rIconPacker text chunk from PNG
                    rpng_chunk chunk = rpng_chunk_read_from_memory(icoImageData, "rIPt");
                    memcpy(entries[i].text, chunk.data, (chunk.length < MAX_IMAGE_TEXT_SIZE)? chunk.length : MAX_IMAGE_TEXT_SIZE - 1);
                    RPNG_FREE(chunk.data);

                    imageCounter++;
                }
            }

            RL_FREE(icoImageData);
        }

        RL_FREE(icoDirEntry);

        fclose(icoFile);
    }

    *count = imageCounter;
    return entries;
}

// Save icon (.ico)
// NOTE: Make sure entries array sizes are valid!
static void SaveIconPackToICO(IconEntry *entries, int entryCount, const char *fileName)
{
    // Verify icon pack valid entries (not placeholder ones)
    int packValidCount = 0;
    for (int i = 0; i < entryCount; i++) if (entries[i].valid) packValidCount++;

    if (packValidCount == 0) return;

    // Define ico file header and entry
    IcoHeader icoHeader = { .reserved = 0, .imageType = 1, .imageCount = packValidCount };
    IcoDirEntry *icoDirEntry = (IcoDirEntry *)RL_CALLOC(icoHeader.imageCount, sizeof(IcoDirEntry));

    char **pngDataPtrs = (char **)RL_CALLOC(icoHeader.imageCount, sizeof(char *));     // Pointers array to PNG image data
    int offset = 6 + 16*icoHeader.imageCount;

    // Get image png data (and pointers to each image)
    // NOTE: In case of PNG export as ZIP, files are directly packed in the loop, one by one
    for (int i = 0, k = 0; i < entryCount; i++)
    {
        if (entries[i].valid)
        {
            int fileSize = 0;           // Store generated png file size (with rIPt chunk)
            int tempFileSize = 0;       // Store generated png file size (no text chunk)
            int colorChannels = 0;

            // Compress entries data into PNG file data streams
            // Image data format could be RGB (3 bytes) instead of RGBA (4 bytes)
            if (entries[i].image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) colorChannels = 3;
            else if (entries[i].image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) colorChannels = 4;

            // NOTE: Memory is allocated internally using RPNG_MALLOC(), must be freed with RPNG_FREE()
            char *tempPngData = rpng_save_image_to_memory(entries[i].image.data, entries[i].image.width, entries[i].image.height, colorChannels, 8, &tempFileSize);

            // Check if exporting text chunks is required
            if (exportTextChunkChecked && (entries[i].text[0] != '\0'))
            {
                // Add image text chunks to generated PNGs
                rpng_chunk chunk = { 0 };
                chunk.data = entries[i].text;
                chunk.length = strlen(entries[i].text);
                memcpy(chunk.type, "rIPt", 4);
                pngDataPtrs[k] = rpng_chunk_write_from_memory(tempPngData, chunk, &fileSize);
            }
            else
            {
                pngDataPtrs[k] = tempPngData;
                fileSize = tempFileSize;
            }

            icoDirEntry[k].width = (entries[i].image.width == 256)? 0 : entries[i].image.width;
            icoDirEntry[k].height = (entries[i].image.width == 256)? 0 : entries[i].image.width;
            icoDirEntry[k].bpp = 32;
            icoDirEntry[k].size = fileSize;
            icoDirEntry[k].offset = offset;

            offset += fileSize;
            k++;
        }
    }

    FILE *icoFile = fopen(fileName, "wb");

    if (icoFile != NULL)
    {
        // Write ico header
        fwrite(&icoHeader, sizeof(IcoHeader), 1, icoFile);

        // Write icon entries entries data
        for (int i = 0; i < icoHeader.imageCount; i++) fwrite(&icoDirEntry[i], sizeof(IcoDirEntry), 1, icoFile);

        // Write icon png data
        for (int i = 0; i < icoHeader.imageCount; i++) fwrite(pngDataPtrs[i], icoDirEntry[i].size, 1, icoFile);

        fclose(icoFile);
    }

    // Free used data (pngs data)
    for (int i = 0; i < icoHeader.imageCount; i++) RPNG_FREE(pngDataPtrs[i]);

    RL_FREE(icoDirEntry);
    RL_FREE(pngDataPtrs);
}

// Save images as .png
static void ExportIconPackImages(IconEntry *entries, int entryCount, const char *fileName)
{
    // Verify icon pack valid entries (not placeholder ones)
    int packValidCount = 0;
    for (int i = 0; i < entryCount; i++) if (entries[i].valid) packValidCount++;

    if (packValidCount == 0) return;

    char **pngDataPtrs = (char **)RL_CALLOC(packValidCount, sizeof(char *));     // Pointers array to PNG image data

    // Get image png data (and pointers to each image)
    // NOTE: In case of PNG export as ZIP, files are directly packed in the loop, one by one
    for (int i = 0, k = 0; i < entryCount; i++)
    {
        if (entries[i].valid)
        {
            int fileSize = 0;           // Store generated png file size (with rIPt chunk)
            int tempFileSize = 0;       // Store generated png file size (no text chunk)
            int colorChannels = 0;

            // Compress entries data into PNG file data streams
            // Image data format could be RGB (3 bytes) instead of RGBA (4 bytes)
            if (entries[i].image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) colorChannels = 3;
            else if (entries[i].image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) colorChannels = 4;

            // NOTE: Memory is allocated internally using RPNG_MALLOC(), must be freed with RPNG_FREE()
            char *tempPngData = rpng_save_image_to_memory(entries[i].image.data, entries[i].image.width, entries[i].image.height, colorChannels, 8, &tempFileSize);

            // Check if exporting text chunks is required
            if (exportTextChunkChecked && (entries[i].text[0] != '\0'))
            {
                // Add image text chunks to generated PNGs
                rpng_chunk chunk = { 0 };
                chunk.data = entries[i].text;
                chunk.length = strlen(entries[i].text);
                memcpy(chunk.type, "rIPt", 4);
                pngDataPtrs[k] = rpng_chunk_write_from_memory(tempPngData, chunk, &fileSize);
            }
            else
            {
                pngDataPtrs[k] = tempPngData;
                fileSize = tempFileSize;
            }

#if defined(EXPORT_IMAGE_PACK_AS_ZIP)
            // Export a single .zip file containing all images
            // Package every image into an output ZIP file (fileName.zip)
            mz_bool status = mz_zip_add_mem_to_archive_file_in_place(TextFormat("%s.zip", fileName), TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(fileName), entries[i].image.width, entries[i].image.height), pngDataPtrs[i], fileSize, NULL, 0, MZ_BEST_SPEED); //MZ_BEST_COMPRESSION, MZ_DEFAULT_COMPRESSION
            if (!status) LOG("WARNING: Zip accumulation process failed\n");
#else
            // Save every PNG file individually
            SaveFileData(TextFormat("%s/%s_%ix%i.png", GetDirectoryPath(fileName), GetFileNameWithoutExt(fileName), entries[i].image.width, entries[i].image.height), pngDataPtrs[i], fileSize);
#endif
            k++;
        }
    }

    // Free used data (pngs data)
    for (int i = 0; i < packValidCount; i++) RPNG_FREE(pngDataPtrs[i]);
    RL_FREE(pngDataPtrs);
}

// Icns data loader
// NOTE: ARGB and JPEG2000 image data formats not supported, only PNG
static IconEntry *LoadIconPackFromICNS(const char *fileName, int *count)
{
    #define MAX_ICNS_IMAGE_SUPPORTED    32

    #define SWAP_INT32(x) (((x) >> 24) | (((x) & 0x00ff0000) >> 8) | (((x) & 0x0000ff00) << 8) | ((x) << 24))

    IconEntry *entries = NULL;
    unsigned int imageCounter = 0;

    FILE *icnsFile = fopen(fileName, "rb");

    if (icnsFile != NULL)
    {
        unsigned char icnsSig[4] = { 0 };
        fread(icnsSig, 1, 4, icnsFile);

        if ((icnsSig[0] == 'i') && (icnsSig[1] == 'c') && (icnsSig[2] == 'n') && (icnsSig[3] == 's'))
        {
            unsigned char icnType[4] = { 0 };
            unsigned int fileSize = 0;
            unsigned int sizeBE = 0;
            fread(&sizeBE, sizeof(unsigned int), 1, icnsFile);
            fileSize = SWAP_INT32(sizeBE);

            // Allocate space for the maximum number of entries supported
            // NOTE: Only the valid loaded images will be filled, some entries will be empty,
            // but the returned entries count will refer to the loaded images
            // There shouldn't be any problem when freeing the pointer...
            entries = (IconEntry *)RL_CALLOC(MAX_ICNS_IMAGE_SUPPORTED, sizeof(IconEntry));

            unsigned int processedSize = 8;

            for (int i = 0; (i < MAX_ICNS_IMAGE_SUPPORTED) && (processedSize < fileSize); i++)
            {
                fread(icnType, 1, 4, icnsFile);

                unsigned int icnSize = 0;
                fread(&sizeBE, sizeof(unsigned int), 1, icnsFile);
                icnSize = SWAP_INT32(sizeBE);

                processedSize += 8;     // IcnType an IcnSize parameters
                icnSize -= 8;           // IcnSize also considers type and size parameters, we must subtract them to get actual data size

                // We have next icn type and size, now we must check if it's a supported format to load it
                LOG("INFO: [%s] ICNS OSType: %c%c%c%c [%i bytes]\n", GetFileName(fileName), icnType[0], icnType[1], icnType[2], icnType[3], icnSize);

                // NOTE: Only supported formats including PNG data
                if (((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 'p') && (icnType[3] == '4')) ||   // 16x16, icp4, not properly displayed on .app
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 'p') && (icnType[3] == '5')) ||   // 32x32, icp5, not properly displayed on .app
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 'p') && (icnType[3] == '6')) ||   // 48x48, icp6, not properly displayed on .app
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '0') && (icnType[3] == '4')) ||   // 16x16, ic04
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 's') && (icnType[3] == 'b')) ||   // 18x18, icsb
                    ((icnType[0] == 's') && (icnType[1] == 'b') && (icnType[2] == '2') && (icnType[3] == '4')) ||   // 24x24, sb24
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 'p') && (icnType[3] == '5')) ||   // 32x32, icp5, not properly displayed on .app
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '0') && (icnType[3] == '5')) ||   // 32x32, ic05 (16x16@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '1') && (icnType[3] == '1')) ||   // 32x32, ic11 (16x16@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 's') && (icnType[3] == 'B')) ||   // 36x36, icsB (18x18@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == 'p') && (icnType[3] == '6')) ||   // 48x48, icp6, not properly displayed on .app
                    ((icnType[0] == 'S') && (icnType[1] == 'B') && (icnType[2] == '2') && (icnType[3] == '4')) ||   // 48x48, SB24 (24x24@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '1') && (icnType[3] == '2')) ||   // 64x64, ic12 (32x32@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '0') && (icnType[3] == '7')) ||   // 128x128, ic07
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '0') && (icnType[3] == '8')) ||   // 256x256, ic08
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '1') && (icnType[3] == '3')) ||   // 256x256, ic13 (128x128@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '0') && (icnType[3] == '9')) ||   // 512x512, ic09
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '1') && (icnType[3] == '4')) ||   // 512x512, ic14 (256x256@2x "retina")
                    ((icnType[0] == 'i') && (icnType[1] == 'c') && (icnType[2] == '1') && (icnType[3] == '0')))     // 1024x1024, ic10 (512x512@2x "retina")
                {
                    // NOTE: We only support loading PNG data, JPEG2000 and ARGB data not supported

                    unsigned char *icnImageData = (unsigned char *)RL_CALLOC(icnSize, 1);
                    fread(icnImageData, 1, icnSize, icnsFile);

                    // Verify PNG signature for loaded image data
                    if ((icnImageData[0] == 0x89) &&
                        (icnImageData[1] == 0x50) &&
                        (icnImageData[2] == 0x4e) &&
                        (icnImageData[3] == 0x47) &&
                        (icnImageData[4] == 0x0d) &&
                        (icnImageData[5] == 0x0a) &&
                        (icnImageData[6] == 0x1a) &&
                        (icnImageData[7] == 0x0a))
                    {
                        // Data contains a valid PNG file, we can load it
                        /*
                        int colors = 0;
                        int bits = 0;
                        int width = 0;
                        int height = 0;
                        char *data = rpng_load_image_from_memory(icnImageData, &width, &height, &colors, &bits); // Image loaded successfully
                        Image image = {
                            .width = width,
                            .height = height,
                            .mipmaps = 1,
                            .data = data,
                            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
                        };
                        entries[imageCounter].image = image;
                        //if (image.width == 512) ExportImage(image, "D:\\testing_512x512.png");    // Image saved successfully, no lost pixels
                        */

                        entries[imageCounter].image = LoadImageFromMemory(".png", icnImageData, icnSize);

                        if ((entries[imageCounter].image.data != NULL) && (entries[imageCounter].image.width != 0))
                        {
                            entries[imageCounter].size = entries[imageCounter].image.width;   // Icon size (expected squared)
                            entries[imageCounter].valid = false;    // Not valid until it is checked against the current package (sizes)
                            entries[imageCounter].generated = false;

                            // Read custom rIconPacker text chunk from PNG
                            rpng_chunk chunk = rpng_chunk_read_from_memory(icnImageData, "rIPt");
                            memcpy(entries[imageCounter].text, chunk.data, (chunk.length < MAX_IMAGE_TEXT_SIZE)? chunk.length : MAX_IMAGE_TEXT_SIZE - 1);
                            RPNG_FREE(chunk.data);

                            imageCounter++;
                        }
                    }
                    else LOG("WARNING: ICNS data format not supported\n");

                    // JPEG2000 data signatures (not supported)
                    // Option 1: 0x00 0x00 0x00 0x0c 0x6a 0x50 0x20 0x20 0x0d 0x0a 0x87 0x0a
                    // Option 2: 0xff 0x4f 0xff 0x51

                    // Useful for the future: ARGB to RGBA
                    /*
                    // Source is in format: 0xAARRGGBB
                    ((x & 0xFF000000) >> 24) | //______AA
                    ((x & 0x00FF0000) >>  8) | //____RR__
                    ((x & 0x0000FF00) <<  8) | //__GG____
                    ((x & 0x000000FF) << 24);  //BB______
                    // Return value is in format:  0xBBGGRRAA   -> ARGB?
                    */

                    RL_FREE(icnImageData);
                }
                else
                {
                    // In case OSType is not supported we just skip the required size
                    fseek(icnsFile, icnSize, SEEK_CUR);
                }

                processedSize += icnSize;
            }

            LOG("INFO: Total images extracted from ICNS file: %i\n", imageCounter);
        }

        fclose(icnsFile);
    }

    *count = imageCounter;
    return entries;
}

// Save icns file (Apple)
// LIMITATIONS:
//  - Supported OS Version: >=10.7
//  - Supported PNG compressed images only
//  - Supported OSTypes [8]: ic11, SB24, ic12, ic07, ic13, ic14, ic10
//  - Supported image sizes [8]: 32, 48, 64, 128, 256, 512, 1024
//  - No TOC or additional chunks supported
//  - Main focus on .app package icns generation
// REF: https://en.wikipedia.org/wiki/Apple_Icon_Image_format
static void SaveIconPackToICNS(IconEntry *entries, int entryCount, const char *fileName)
{
    // Verify icon pack valid entries (not placeholder ones)
    int packValidCount = 0;
    for (int i = 0; i < entryCount; i++) if (entries[i].valid) packValidCount++;
    if (packValidCount == 0) return;
/*
    // NOTE: This validation is not required because it is already done when
    // adding icons from input files into icon package entries

    #define ICNS_MAX_SUPPORTED_IMAGES   8

    // Validate input entries image data to get only supported sizes
    int validCount = 0;
    int validIndex[ICNS_MAX_SUPPORTED_IMAGES] = { -1 };
    for (int i = 0; (i < entryCount) && (validCount < ICNS_MAX_SUPPORTED_IMAGES); i++)
    {
        if ((entries[i].image.width == 16) && (entries[i].image.height == 16)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 32) && (entries[i].image.height == 32)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 48) && (entries[i].image.height == 48)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 64) && (entries[i].image.height == 64)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 128) && (entries[i].image.height == 128)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 256) && (entries[i].image.height == 256)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 512) && (entries[i].image.height == 512)) { validIndex[validCount] = i; validCount++; }
        else if ((entries[i].image.width == 1024) && (entries[i].image.height == 1024)) { validIndex[validCount] = i; validCount++; }
    }
*/
    // Compress provided images into PNG data
    char **pngDataPtrs = (char **)RL_CALLOC(packValidCount, sizeof(char *));     // Pointers array to PNG image data
    unsigned int *pngDataSizes = (unsigned int *)RL_CALLOC(packValidCount, sizeof(unsigned int));   // PNG data size

    // Get image png data (and pointers to each image)
    // NOTE: In case of PNG export as ZIP, files are directly packed in the loop, one by one
    for (int i = 0, k = 0; i < entryCount; i++)
    {
        if (entries[i].valid)
        {
            // Compress entries data into PNG file data streams
            // Image data format could be RGB (3 bytes) instead of RGBA (4 bytes)
            int colorChannels = 0;
            if (entries[i].image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) colorChannels = 3;
            else if (entries[i].image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) colorChannels = 4;

            // NOTE: Memory is allocated internally using RPNG_MALLOC(), must be freed with RPNG_FREE()
            pngDataPtrs[k] = rpng_save_image_to_memory(entries[i].image.data, entries[i].image.width, entries[i].image.height, colorChannels, 8, &pngDataSizes[k]);

            k++;
        }
    }

    // We got the images converted to PNG in memory, now we can create the icns file

    FILE *icnsFile = fopen(fileName, "wb");

    if (icnsFile != NULL)
    {
        /*
        // Data structures to know how data is organized inside the .icns file

        // Icns File Header (8 bytes)
        typedef struct {
        unsigned char id[4];        // Magic literal: "icns" (0x69, 0x63, 0x6e, 0x73)
        unsigned int size;          // Length of file, in bytes, MSB first (Big Endian)
        } IcnsHeader;

        for (int i = 0; i < count; i++)
        {
        // Icon Entry info (8 bytes + data)
        typedef struct {
        unsigned char type[4];      // Icon type, defined by OSType (Big Endian)
        unsigned int dataSize;      // Length of data, in bytes (including type and length), MSB first
        unsigned char *data;        // Icon data
        } IcnsEntry;
        }
        */

        // Write icns header signature
        // unsigned char icnsId[4] = { 0x69, 0x63, 0x6e, 0x73 };     // "icns"
        fwrite("icns", 1, 4, icnsFile);

        // ICNS file size, all file including header,
        // We init it with expected chunck size but
        // we need to accumulate every generated PNG size
        unsigned int icnsFileSize = 8 + 8*packValidCount;
        for (int i = 0; i < packValidCount; i++) icnsFileSize += pngDataSizes[i];
        unsigned char sizeBE[4] = { 0 };

        // Write icns total data size (Big Endian)
        sizeBE[0] = (icnsFileSize >> 24) & 0xff;
        sizeBE[1] = (icnsFileSize >> 16) & 0xff;
        sizeBE[2] = (icnsFileSize >> 8) & 0xff;
        sizeBE[3] = icnsFileSize & 0xff;
        fwrite(sizeBE, 1, 4, icnsFile);

        unsigned char icnType[4] = { 0 };

        // Write icns entries
        for (int i = 0, k = 0; i < entryCount; i++)
        {
            if (entries[i].valid)
            {
                switch (entries[i].image.width)
                {
                    case 16:   { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = 'p'; icnType[3] = '4'; } break;   // icp4, not properly displayed on .app
                    //case 32: { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = 'p'; icnType[3] = '5'; } break;   // icp5, not properly displayed on .app
                    case 32:   { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '1'; icnType[3] = '1'; } break;   // ic11 (16x16@2x "retina")
                    //case 48: { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = 'p'; icnType[3] = '6'; } break;   // icp6, not properly displayed on .app
                    case 48:   { icnType[0] = 'S'; icnType[1] = 'B'; icnType[2] = '2'; icnType[3] = '4'; } break;   // SB24 (24x24@2x "retina")
                    case 64:   { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '1'; icnType[3] = '2'; } break;   // ic12 (32x32@2x "retina")
                    case 128:  { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '0'; icnType[3] = '7'; } break;   // ic07
                   //case 256: { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '0'; icnType[3] = '8'; } break;   // ic08
                    case 256:  { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '1'; icnType[3] = '3'; } break;   // ic13 (128x128@2x "retina")
                   //case 512: { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '0'; icnType[3] = '9'; } break;   // ic09
                    case 512:  { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '1'; icnType[3] = '4'; } break;   // ic14 (256x256@2x "retina")
                    case 1024: { icnType[0] = 'i'; icnType[1] = 'c'; icnType[2] = '1'; icnType[3] = '0'; } break;   // ic10 (512x512@2x "retina")
                    default: LOG("WARNING: Image size for ICNS generation not supported!\n"); break;
                }

                // Write entry type
                fwrite(icnType, 1, 4, icnsFile);

                // Write entry size (Big endian)
                unsigned int size = pngDataSizes[k] + 8;   // Size must include type and length size
                sizeBE[0] = (size >> 24) & 0xff;
                sizeBE[1] = (size >> 16) & 0xff;
                sizeBE[2] = (size >> 8) & 0xff;
                sizeBE[3] = size & 0xff;
                fwrite(sizeBE, 1, 4, icnsFile);

                // Write entry PNG icon data
                fwrite(pngDataPtrs[k], pngDataSizes[k], 1, icnsFile);

                k++;
            }
        }

        fclose(icnsFile);
    }

    // Free used data (pngs data)
    for (int i = 0; i < packValidCount; i++) RPNG_FREE(pngDataPtrs[i]);

    RL_FREE(pngDataPtrs);
    RL_FREE(pngDataSizes);
}

// Get text lines available on icon pack
// NOTE: Only valid icons considered
static unsigned int CountIconPackTextLines(IconPack pack)
{
    //static const char *textLines[MAX_PACK_ELEMENTS] = { 0 }; // Pointers array to possible text lines
    unsigned int counter = 0;

    for (int i = 0; i < pack.count; i++)
    {
        if (pack.entries[i].valid && (strlen(pack.entries[i].text) > 0))
        {
            //textLines[counter] = pack.entries[i].text;
            counter++;
        }
    }

    return counter;
}

// Add icon to bucket
static void AddIconToBucket(IconBucket *bucket, const char *fileName)
{
    IconEntry *entries = NULL;
    int imageCount = 0;

    // Load all available entries
    if (IsFileExtension(fileName, ".ico")) entries = LoadIconPackFromICO(fileName, &imageCount);
    if (IsFileExtension(fileName, ".icns")) entries = LoadIconPackFromICNS(fileName, &imageCount);
    else if (IsFileExtension(fileName, ".png;.bmp;.qoi"))
    {
        Image image = LoadImage(fileName);

        // Minimal image validation
        if ((image.data != NULL) && (image.width <= 1024) && (image.width == image.height))
        {
            imageCount = 1;
            entries = (IconEntry *)RL_CALLOC(imageCount, sizeof(IconEntry));
            entries[0].image = image;
            entries[0].size = image.width;

            // Try to find rIPt text lines
            if (IsFileExtension(fileName, ".png"))
            {
                // Read custom rIconPacker text chunk from PNG
                rpng_chunk chunk = rpng_chunk_read(fileName, "rIPt");
                if (chunk.length > 0) memcpy(entries[0].text, chunk.data, (chunk.length < MAX_IMAGE_TEXT_SIZE)? chunk.length : MAX_IMAGE_TEXT_SIZE - 1);
                RPNG_FREE(chunk.data);
            }
        }
        else UnloadImage(image);
    }

    int dupIndex = -1;

    // Add new entries to bucket
    for (int i = 0; (i < imageCount) && (bucket->count < bucket->capacity); i++)
    {
        // Check if bucket already contains an image with same size
        for (int k = 0; k < bucket->count; k++)
        {
            if (entries[i].size == bucket->entries[k].size)
            {
                // Found bucket entry with same size -> replace bucket entry!
                dupIndex = k;
                break;
            }
        }

        if (dupIndex > -1)
        {
            // Unload current entry
            UnloadImage(bucket->entries[dupIndex].image);
            memset(bucket->entries[dupIndex].text, 0, MAX_IMAGE_TEXT_SIZE);

            // Update with new entry
            bucket->entries[dupIndex] = entries[i];
            if (entries[i].text[0] != '\0') memcpy(bucket->entries[dupIndex].text, entries[i].text, MAX_IMAGE_TEXT_SIZE);
            dupIndex = -1;
        }
        else
        {
            bucket->entries[bucket->count] = entries[i];
            if (entries[i].text[0] != '\0') memcpy(bucket->entries[bucket->count].text, entries[i].text, MAX_IMAGE_TEXT_SIZE);
            bucket->count++;
        }
    }

    RL_FREE(entries);
}

// Remove icon from bucket
static void RemoveIconFromBucket(IconBucket *bucket, unsigned int size)
{
    // TODO: Remove bucket icon... really required?
}

// Clear icon bucket
static void ClearIconBucket(IconBucket *bucket)
{
    for (int i = 0; i < bucket->count; i++)
    {
        UnloadImage(bucket->entries[i].image);
        bucket->entries[i] = (IconEntry){ 0 };
    }

    bucket->count = 0;
}

// NOTE: Platform determines the requested sizes
static void UpdateIconPackFromBucket(IconPack *pack, IconBucket bucket)
{
    for (int i = 0; i < bucket.count; i++)
    {
        for (int k = 0; k < pack->count; k++)
        {
            if (bucket.entries[i].size == pack->entries[k].size)
            {
                if (pack->entries[k].generated) UnloadImage(pack->entries[k].image);

                pack->entries[k] = bucket.entries[i];

                UnloadTexture(pack->textures[k]);
                pack->textures[k] = (Texture2D){ 0 };
                pack->textures[k] = LoadTextureFromImage(pack->entries[k].image);

                pack->entries[k].valid = true;
                pack->entries[k].generated = false;
            }
        }
    }
}

// Reset icon pack data
static void ResetIconPack(IconPack *pack, int platform)
{
    // Clear full pack
    for (int i = 0; i < MAX_PACK_ELEMENTS; i++)
    {
        if (pack->entries[i].generated) UnloadImage(pack->entries[i].image);
        else pack->entries[i].image = (Image){ 0 };      // Remove bucket image (not unload)

        UnloadTexture(pack->textures[i]);
        pack->textures[i] = (Texture2D){ 0 };

        memset(pack->entries[i].text, 0, MAX_IMAGE_TEXT_SIZE);
        pack->entries[i].generated = false;
        pack->entries[i].valid = false;
        pack->entries[i].size = 0;
    }

    // Reset to required platform
    unsigned int *platformSizes = NULL;
    switch (platform)
    {
        case ICON_PLATFORM_WINDOWS: pack->count = 8; platformSizes = icoSizesWindows; break;
        case ICON_PLATFORM_MACOS: pack->count = 8; platformSizes = icoSizesMacOS; break;
        case ICON_PLATFORM_FAVICON: pack->count = 10; platformSizes = icoSizesFavicon; break;
        case ICON_PLATFORM_ANDROID: pack->count = 10; platformSizes = icoSizesAndroid; break;
        case ICON_PLATFORM_IOS7: pack->count = 9; platformSizes = icoSizesiOS; break;
        default: break;
    }

    for (int i = 0; i < pack->count; i++) pack->entries[i].size = platformSizes[i];
}