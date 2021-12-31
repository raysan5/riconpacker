/*******************************************************************************************
*
*   rIconPacker v1.5 - A simple and easy-to-use icons packer
*
*   CONFIGURATION:
*
*   #define VERSION_ONE
*       Enable command-line usage and PRO features for the tool
*
*   #define COMMAND_LINE_ONLY
*       Compile tool only for command line usage
*
*   #define CUSTOM_MODAL_DIALOGS
*       Use custom raygui generated modal dialogs instead of native OS ones
*       NOTE: Avoids including tinyfiledialogs depencency library
*
*   VERSIONS HISTORY:
*       1.5  (30-Dec-2021) Updated to raylib 4.0 and raygui 3.1
*       1.0  (23-Mar-2019) First release
*
*   DEPENDENCIES:
*       raylib 4.0              - Windowing/input management and drawing
*       raygui 3.1              - Immediate-mode GUI controls with custom styling and icons
*       rpng 1.0                - PNG chunks management
*       tinyfiledialogs 3.8.8   - Open/save file dialogs, it requires linkage with comdlg32 and ole32 libs
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
*
*   LICENSE: Propietary License
*
*   Copyright (c) 2018-2022 raylib technologies (@raylibtech). All Rights Reserved.
*
*   Unauthorized copying of this file, via any medium is strictly prohibited
*   This project is proprietary and confidential unless the owner allows
*   usage in any other form by expresely written permission.
*
**********************************************************************************************/

#define TOOL_NAME               "rIconPacker"
#define TOOL_SHORT_NAME         "rIP"
#define TOOL_VERSION            "1.5"
#define TOOL_DESCRIPTION        "A simple and easy-to-use icons packer"
#define TOOL_RELEASE_DATE       "Dec.2021"
#define TOOL_LOGO_COLOR         0xffc800ff

#include "raylib.h"

#define RPNG_IMPLEMENTATION
#include "external/rpng.h"                  // PNG chunks management

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                         // Required for: IMGUI controls

#undef RAYGUI_IMPLEMENTATION                // Avoid including raygui implementation again

#define GUI_WINDOW_ABOUT_IMPLEMENTATION
#include "gui_window_about.h"               // GUI: About window

#define GUI_FILE_DIALOGS_IMPLEMENTATION
#include "gui_file_dialogs.h"               // GUI: File Dialogs

#include <stdio.h>                      // Required for: fopen(), fclose(), fread()...
#include <stdlib.h>                     // Required for: malloc(), free()
#include <string.h>                     // Required for: strcmp(), strlen()
#include <math.h>                       // Required for: ceil()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#if (!defined(_DEBUG) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
bool __stdcall FreeConsole(void);       // Close console from code (kernel32.lib)
#endif

// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
  #define LOG(...) printf(__VA_ARGS__)
#else
  #define LOG(...)
#endif

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

// Icon pack
typedef struct {
    IconPackEntry *icons;       // Pack icons
    unsigned int *sizes;        // Icon sizes pointer
    int count;                  // Pack icon count
} IconPack;

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
static const char *toolName = TOOL_NAME;
static const char *toolVersion = TOOL_VERSION;
static const char *toolDescription = TOOL_DESCRIPTION;

// NOTE: Default icon sizes by platform: http://iconhandbook.co.uk/reference/chart/
static unsigned int icoSizesWindows[8] = { 256, 128, 96, 64, 48, 32, 24, 16 };              // Windows app icons
static unsigned int icoSizesFavicon[10] = { 228, 152, 144, 120, 96, 72, 64, 32, 24, 16 };   // Favicon for multiple devices
static unsigned int icoSizesAndroid[10] = { 192, 144, 96, 72, 64, 48, 36, 32, 24, 16 };     // Android Launcher/Action/Dialog/Others icons, missing: 512
static unsigned int icoSizesiOS[9] = { 180, 152, 120, 87, 80, 76, 58, 40, 29 };             // iOS App/Settings/Others icons, missing: 512, 1024

#if !defined(COMMAND_LINE_ONLY)
static IconPack packs[4] = { 0 };           // Icon packs, one for every platform
static int sizeListActive = 0;              // Current list text entry
static int validCount = 0;                  // Valid ico images counter
#endif

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#if defined(VERSION_ONE) || defined(COMMAND_LINE_ONLY)
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

#if !defined(COMMAND_LINE_ONLY)
// Icon pack management functions
static IconPack LoadIconPack(int platform);     // Load icon pack for a specific platform
static void UnloadIconPack(IconPack *pack);     // Unload icon pack

static void LoadIconToPack(IconPack *pack, const char *fileName); // Load icon file into pack
static void UnloadIconFromPack(IconPack *pack, int index);        // Unload one icon from the pack

static char *GetTextIconSizes(IconPack pack);   // Get sizes as a text array separated by semicolon (ready for GuiListView())
#endif

// Load/Save/Export data functions
static Image *LoadICO(const char *fileName, int *count);    // Load icon data
static void SaveICO(Image *images, int imageCount, const char *fileName);  // Save icon data


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    char inFileName[512] = { 0 };       // Input file name (required in case of drag & drop over executable)
    char outFileName[512] = { 0 };      // Output file name (required for file save/export)

#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messsages
#endif
#if defined(COMMAND_LINE_ONLY)
    ProcessCommandLine(argc, argv);
#else
#if defined(VERSION_ONE)
    // Command-line usage mode
    //--------------------------------------------------------------------------------------
    if (argc > 1)
    {
        if ((argc == 2) &&
            (strcmp(argv[1], "-h") != 0) &&
            (strcmp(argv[1], "--help") != 0))       // One argument (file dropped over executable?)
        {
            if (IsFileExtension(argv[1], ".ico") ||
                IsFileExtension(argv[1], ".png"))
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
#endif      // VERSION_ONE
#if (!defined(_DEBUG) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
    // WARNING (Windows): If program is compiled as Window application (instead of console),
    // no console is available to show output info... solution is compiling a console application
    // and closing console (FreeConsole()) when changing to GUI interface
    FreeConsole();
#endif

    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 400;
    const int screenHeight = 380;

    InitWindow(screenWidth, screenHeight, TextFormat("%s v%s", toolName, toolVersion));
    SetExitKey(0);

    // General pourpose variables
    //Vector2 mousePoint = { 0.0f, 0.0f };
    int framesCounter = 0;

    // Initialize all icon packs (for all platforms)
    packs[0] = LoadIconPack(ICON_PLATFORM_WINDOWS);
    packs[1] = LoadIconPack(ICON_PLATFORM_FAVICON);
    packs[2] = LoadIconPack(ICON_PLATFORM_ANDROID);
    packs[3] = LoadIconPack(ICON_PLATFORM_IOS7);

    // GUI: Main Layout
    //----------------------------------------------------------------------------------
    Vector2 anchorMain = { 0, 0 };

    int platformActive = 0;
    int prevPlatformActive = 0;
    int scaleAlgorythmActive = 1;

    bool btnGenIconImagePressed = false;
    bool btnClearIconImagePressed = false;
    bool btnSaveImagePressed = false;

    GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 24);
    //----------------------------------------------------------------------------------

    // GUI: About Window
    //-----------------------------------------------------------------------------------
    GuiWindowAboutState windowAboutState = InitGuiWindowAbout();
    //-----------------------------------------------------------------------------------

    // GUI: Exit Window
    //-----------------------------------------------------------------------------------
    bool exitWindow = false;
    bool windowExitActive = false;
    //-----------------------------------------------------------------------------------

    // GUI: Custom file dialogs
    //-----------------------------------------------------------------------------------
    bool showLoadFileDialog = false;
    bool showExportFileDialog = false;
    bool showExportImageDialog = false;
    //-----------------------------------------------------------------------------------

    // Check if an icon input file has been provided on command line
    if (inFileName[0] != '\0') LoadIconToPack(&packs[platformActive], inFileName);

    SetTargetFPS(120);      // Increased for smooth pixel painting on edit mode
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!exitWindow)     // Detect window close button or ESC key
    {
        if (WindowShouldClose()) windowExitActive = true;

        // Dropped files logic
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            int dropsCount = 0;
            char **droppedFiles = GetDroppedFiles(&dropsCount);

            // Support gui styles
            if ((dropsCount == 1) && IsFileExtension(droppedFiles[0], ".rgs")) GuiLoadStyle(droppedFiles[0]);

            for (int i = 0; i < dropsCount; i++)
            {
                if (IsFileExtension(droppedFiles[i], ".ico") ||
                    IsFileExtension(droppedFiles[i], ".png"))
                {
                    // Load images into IconPack
                    LoadIconToPack(&packs[platformActive], droppedFiles[i]);
                }
            }

            ClearDroppedFiles();
        }
        //----------------------------------------------------------------------------------

        // Keyboard shortcuts
        //------------------------------------------------------------------------------------

        // Show dialog: load input file (.ico, .png)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) showLoadFileDialog = true;

        // Show dialog: save icon file (.ico)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E))
        {
            if (validCount > 0) showExportFileDialog = true;
        }

        // Show dialog: export icon data
        if ((IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) || btnSaveImagePressed)
        {
            if ((sizeListActive > 0) && (packs[platformActive].icons[sizeListActive - 1].valid)) showExportImageDialog = true;
        }

        // Show window: about
        if (IsKeyPressed(KEY_F1)) windowAboutState.windowActive = true;

        // Delete selected icon from list
        if (IsKeyPressed(KEY_DELETE) || btnClearIconImagePressed)
        {
            if (sizeListActive == 0) for (int i = 0; i < packs[platformActive].count; i++) UnloadIconFromPack(&packs[platformActive], i);  // Delete all images in the series
            else UnloadIconFromPack(&packs[platformActive], sizeListActive - 1);                                            // Delete one image
        }

        // Generate icon
        if (IsKeyPressed(KEY_SPACE))
        {
            // Force icon regeneration if possible
            if (validCount > 0) btnGenIconImagePressed = true;
        }

        // Show closing window on ESC
        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (windowAboutState.windowActive) windowAboutState.windowActive = false;
#if !defined(PLATFORM_WEB)
            else windowExitActive = !windowExitActive;
#endif
        }
        //----------------------------------------------------------------------------------

        // Basic program flow logic
        //----------------------------------------------------------------------------------
        framesCounter++;                    // General usage frames counter

        // Calculate valid images
        validCount = 0;
        for (int i = 0; i < packs[platformActive].count; i++) if (packs[platformActive].icons[i].valid) validCount++;

        // Generate new icon image (using biggest available image)
        if (btnGenIconImagePressed)
        {
            // Get bigger available icon
            int biggerValidSize = -1;
            for (int i = 0; i < packs[platformActive].count; i++)
            {
                if (packs[platformActive].icons[i].valid) { biggerValidSize = i; break; }
            }

            if (biggerValidSize >= 0)
            {
                if (sizeListActive == 0)
                {
                    // Generate all missing images in the series
                    for (int i = 0; i < packs[platformActive].count; i++)
                    {
                        if (!packs[platformActive].icons[i].valid)
                        {
                            UnloadImage(packs[platformActive].icons[i].image);
                            packs[platformActive].icons[i].image = ImageCopy(packs[platformActive].icons[biggerValidSize].image);

                            if (scaleAlgorythmActive == 0) ImageResizeNN(&packs[platformActive].icons[i].image, packs[platformActive].icons[i].size, packs[platformActive].icons[i].size);
                            else if (scaleAlgorythmActive == 1) ImageResize(&packs[platformActive].icons[i].image, packs[platformActive].icons[i].size, packs[platformActive].icons[i].size);

                            UnloadTexture(packs[platformActive].icons[i].texture);
                            packs[platformActive].icons[i].texture = LoadTextureFromImage(packs[platformActive].icons[i].image);

                            packs[platformActive].icons[i].valid = true;
                        }
                    }
                }
                else
                {
                    if (!packs[platformActive].icons[sizeListActive - 1].valid)
                    {
                        UnloadImage(packs[platformActive].icons[sizeListActive - 1].image);
                        packs[platformActive].icons[sizeListActive - 1].image = ImageCopy(packs[platformActive].icons[biggerValidSize].image);

                        if (scaleAlgorythmActive == 0) ImageResizeNN(&packs[platformActive].icons[sizeListActive - 1].image, packs[platformActive].icons[sizeListActive - 1].size, packs[platformActive].icons[sizeListActive - 1].size);
                        else if (scaleAlgorythmActive == 1) ImageResize(&packs[platformActive].icons[sizeListActive - 1].image, packs[platformActive].icons[sizeListActive - 1].size, packs[platformActive].icons[sizeListActive - 1].size);

                        UnloadTexture(packs[platformActive].icons[sizeListActive - 1].texture);
                        packs[platformActive].icons[sizeListActive - 1].texture = LoadTextureFromImage(packs[platformActive].icons[sizeListActive - 1].image);

                        packs[platformActive].icons[sizeListActive - 1].valid = true;
                    }
                }
            }
        }

        // Change active platform icons pack
        if (platformActive != prevPlatformActive)
        {
            // TODO: Check icons that can be populated from current platform to next platform

            prevPlatformActive = platformActive;
        }

        // WARNING: Some windows should lock the main screen controls when shown
        if (windowAboutState.windowActive || windowExitActive || showLoadFileDialog || showExportFileDialog || showExportImageDialog) GuiLock();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            // GUI: Main Layout
            //----------------------------------------------------------------------------------
            GuiPanel((Rectangle){ anchorMain.x + 0, anchorMain.y + 0, 400, 45 });

            // Icon platform scheme selector
            platformActive = GuiComboBox((Rectangle){ anchorMain.x + 10, anchorMain.y + 10, 115, 25 }, "Windows;Favicon;Android;iOS", platformActive);

            if (GuiButton((Rectangle){ anchorMain.x + 305, anchorMain.y + 10, 85, 25 }, "#191#ABOUT")) windowAboutState.windowActive = true;
            if (GuiButton((Rectangle){ anchorMain.x + 135, anchorMain.y + 320, 80, 25 }, "#8#Load")) showLoadFileDialog = true;

            sizeListActive = GuiListView((Rectangle){ anchorMain.x + 10, anchorMain.y + 55, 115, 290 }, GetTextIconSizes(packs[platformActive]), NULL, sizeListActive);
            if (sizeListActive < 0) sizeListActive = 0;

            // Draw icons panel and border lines
            //--------------------------------------------------------------------------------------------------------------
            GuiDummyRec((Rectangle){ anchorMain.x + 135, anchorMain.y + 55, 256, 256 }, NULL);
            DrawRectangleLines((int)anchorMain.x + 135, (int)anchorMain.y + 55, 256, 256, Fade(GRAY, 0.6f));

            if (sizeListActive == 0)
            {
                for (int i = 0; i < packs[platformActive].count; i++) DrawTexture(packs[platformActive].icons[i].texture, (int)anchorMain.x + 135, (int)anchorMain.y + 55, WHITE);
            }
            else if (sizeListActive > 0)
            {
                DrawTexture(packs[platformActive].icons[sizeListActive - 1].texture,
                            (int)anchorMain.x + 135 + 128 - packs[platformActive].icons[sizeListActive - 1].texture.width/2,
                            (int)anchorMain.y + 55 + 128 - packs[platformActive].icons[sizeListActive - 1].texture.height/2, WHITE);
            }
            //--------------------------------------------------------------------------------------------------------------

            // NOTE: Enabled buttons depend on several circunstances

            if ((validCount == 0) || ((sizeListActive > 0) && !packs[platformActive].icons[sizeListActive - 1].valid)) GuiDisable();
            btnClearIconImagePressed = GuiButton((Rectangle){ anchorMain.x + 220, anchorMain.y + 320, 80, 25 }, "#9#Clear");
            GuiEnable();

            if ((validCount == 0) || ((sizeListActive > 0) && packs[platformActive].icons[sizeListActive - 1].valid)) GuiDisable();
            btnGenIconImagePressed = GuiButton((Rectangle){ anchorMain.x + 305, anchorMain.y + 320, 85, 25 }, "#12#Generate");
            GuiEnable();

            if ((validCount == 0) || (sizeListActive == 0) || ((sizeListActive > 0) && !packs[platformActive].icons[sizeListActive - 1].valid)) GuiDisable();
            btnSaveImagePressed = GuiButton((Rectangle){ anchorMain.x + 220, anchorMain.y + 10, 80, 25 }, "#12#Save");
            GuiEnable();

            if (validCount == 0) GuiDisable();
            if (GuiButton((Rectangle){ anchorMain.x + 135, anchorMain.y + 10, 80, 25 }, "#7#Export")) showExportFileDialog = true;
            GuiEnable();

            // Draw status bar info
            // TODO: Status information seems redundant... maybe other kind of information could be shown.
            GuiStatusBar((Rectangle){ anchorMain.x + 0, anchorMain.y + 355, 125, 25 }, (sizeListActive == 0)? "SELECTED: ALL" : TextFormat("SELECTED: %ix%i", packs[platformActive].icons[sizeListActive - 1].size, packs[platformActive].icons[sizeListActive - 1].size));
            GuiStatusBar((Rectangle){ anchorMain.x + 124, anchorMain.y + 355, 276, 25 }, (sizeListActive == 0)? TextFormat("AVAILABLE: %i/%i", validCount, packs[platformActive].count) : TextFormat("AVAILABLE: %i/1", packs[platformActive].icons[sizeListActive - 1].valid));

            // NOTE: If some overlap window is open and main window is locked, we draw a background rectangle
            if (GuiIsLocked()) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.85f));

            // WARNING: Before drawing the windows, we unlock them
            GuiUnlock();

            // GUI: About Window
            //-----------------------------------------------------------------------------------
            GuiWindowAbout(&windowAboutState);
            //-----------------------------------------------------------------------------------

            // GUI: Exit Window
            //----------------------------------------------------------------------------------------
            if (windowExitActive)
            {
                int message = GuiMessageBox((Rectangle){ GetScreenWidth()/2.0f - 125, GetScreenHeight()/2.0f - 50, 250, 100 }, "#159#Closing rIconPacker", "Do you really want to exit?", "Yes;No");

                if ((message == 0) || (message == 2)) windowExitActive = false;
                else if (message == 1) exitWindow = true;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Load File Dialog (and loading logic)
            //----------------------------------------------------------------------------------------
            if (showLoadFileDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Load icon or image file", inFileName, "Ok", "Just drag and drop your file!");
#else
                int result = GuiFileDialog(DIALOG_OPEN, "Load icon or image file...", inFileName, "*.ico;*.png", "Icon or Image Files (*.ico, *.png)");
#endif
                if (result == 1) LoadIconToPack(&packs[platformActive], inFileName);   // Load icon file

                if (result >= 0) showLoadFileDialog = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export File Dialog (and saving logic)
            //----------------------------------------------------------------------------------------
            if (showExportFileDialog)
            {
                strcpy(outFileName, "icon.ico");

#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_TEXTINPUT, "Export icon file...", outFileName, "Ok;Cancel", NULL);
#else
                int result = GuiFileDialog(DIALOG_SAVE, "Export icon file...", outFileName, "*.ico", "Icon File (*.ico)");
#endif
                if (result == 1)
                {
                    // Check for valid extension and make sure it is
                    if ((GetFileExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".ico")) strcat(outFileName, ".ico\0");

                    // Verify icon pack valid images (not placeholder ones)
                    int validCount = 0;
                    for (int i = 0; i < packs[platformActive].count; i++) if (packs[platformActive].icons[i].valid) validCount++;

                    Image *images = (Image *)calloc(validCount, sizeof(Image));

                    int imCount = 0;
                    for (int i = 0; i < packs[platformActive].count; i++)
                    {
                        if (packs[platformActive].icons[i].valid)
                        {
                            images[imCount] = packs[platformActive].icons[i].image;
                            imCount++;
                        }
                    }
                    
                    // Check for valid extension and make sure it is
                    if ((GetFileExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".ico")) strcat(outFileName, ".ico\0");
                    SaveICO(images, imCount, outFileName);      // Export valid images to output ICO file

                    free(images);

                #if defined(PLATFORM_WEB)
                    // Download file from MEMFS (emscripten memory filesystem)
                    // NOTE: Second argument must be a simple filename (we can't use directories)
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", outFileName, GetFileName(outFileName)));
                #endif
                }

                if (result >= 0) showExportFileDialog = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export Image Dialog (and saving logic)
            //----------------------------------------------------------------------------------------
            if (showExportImageDialog)
            {
                strcpy(outFileName, TextFormat("icon_%ix%i.png", packs[platformActive].icons[sizeListActive - 1].image.width, packs[platformActive].icons[sizeListActive - 1].image.height));

#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_TEXTINPUT, "Export image file...", outFileName, "Ok;Cancel", NULL);
#else
                int result = GuiFileDialog(DIALOG_SAVE, "Export image file...", outFileName, "*.png", "Image File (*.png)");
#endif
                if (result == 1)
                {
                    // Check for valid extension and make sure it is
                    if ((GetFileExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".png")) strcat(outFileName, ".png\0");

                    ExportImage(packs[platformActive].icons[sizeListActive - 1].image, outFileName);

                #if defined(PLATFORM_WEB)
                    // Download file from MEMFS (emscripten memory filesystem)
                    // NOTE: Second argument must be a simple filename (we can't use directories)
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", outFileName, GetFileName(outFileName)));
                #endif
                }

                if (result >= 0) showExportImageDialog = false;
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload icon packs data
    for (int i = 0; i < 4; i++) UnloadIconPack(&packs[i]);

    CloseWindow();      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

#endif      // !COMMAND_LINE_ONLY

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module functions definition
//--------------------------------------------------------------------------------------------
#if defined(VERSION_ONE) || defined(COMMAND_LINE_ONLY)
// Show command line usage info
static void ShowCommandLineInfo(void)
{
    printf("\n////////////////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                                        //\n");
    printf("// %s v%s - %s                 //\n", toolName, toolVersion, toolDescription);
    printf("// powered by raylib v%s and raygui v%s                                             //\n", RAYLIB_VERSION, RAYGUI_VERSION);
    printf("// more info and bugs-report: ray[at]raylibtech.com                                       //\n");
    printf("//                                                                                        //\n");
    printf("// Copyright (c) 2018-2022 raylib technologies (@raylibtech)                              //\n");
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
    printf("                                      Supported extensions: .png, .ico\n\n");
    printf("    -o, --output <filename.ico>     : Define output icon file.\n");
    printf("                                      NOTE: If not specified, defaults to: output.ico\n\n");
    printf("    -op, --out-platform <value>     : Define out sizes by platform scheme.\n");
    printf("                                      Supported values:\n");
    printf("                                          1 - Windows (Sizes: 256, 128, 96, 64, 48, 32, 24, 16)\n");
    printf("                                          2 - Favicon (Sizes: 228, 152, 144, 120, 96, 72, 64, 32, 24, 16)\n");
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
    char outFileName[256] = { 0 };      // Output file name

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
                const char **files = TextSplit(argv[i + 1], ',', &inputFilesCount);

                inputFiles = (char **)malloc(inputFilesCount);
                for (int j = 0; j < inputFilesCount; j++)
                {
                    inputFiles[j] = (char *)malloc(256);    // Input file name
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
                if (IsFileExtension(argv[i + 1], ".ico"))
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
                const char **values = TextSplit(argv[i + 1], ',', &numValues);

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

                if ((platform > 0) && (platform < 5)) outPlatform = platform;
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
                const char **values = TextSplit(argv[i + 1], ',', &numValues);

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
        if (outFileName[0] == '\0') strcpy(outFileName, "output.ico");  // Set a default name for output in case not provided

        printf("\nInput files:      %s", inputFiles[0]);
        for (int i = 1; i < inputFilesCount; i++) printf(",%s", inputFiles[i]);
        printf("\nOutput file:      %s\n\n", outFileName);

        #define MAX_ICONS_PACK      64

        IconPackEntry inputPack[MAX_ICONS_PACK] = { 0 }; // Icon images array
        int inputPackCount = 0;                          // Icon images array counter

        printf(" > PROCESSING INPUT FILES\n");

        // Load input files (all of them) into icon pack, if one size has been previously loaded, do not load again
        for (int i = 0; i < inputFilesCount; i++)
        {
            int imCount = 0;
            Image *images = NULL;

            // Load all available images in current file
            if (IsFileExtension(inputFiles[i], ".ico")) images = LoadICO(inputFiles[i], &imCount);
            else if (IsFileExtension(inputFiles[i], ".png"))
            {
                imCount = 1;
                images = (Image *)malloc(imCount*sizeof(Image));
                images[0] = LoadImage(inputFiles[i]);
            }

            printf("\nInput file: %s / Images loaded: %i\n", inputFiles[i], imCount);

            // Process all loaded images
            for (int j = 0; j < imCount; j++)
            {
                printf(" > Processing image: %i ", j);

                // Check if provided image size is valid (only squared images supported)
                if (images[j].width != images[j].height) printf("WARNING: Image is not squared as expected (%i x %i)", images[j].width, images[j].height);
                else
                {
                    // TODO: Check if current image size is already available in the package!

                    // Force image to be RGBA
                    ImageFormat(&images[j], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

                    inputPack[inputPackCount].image = ImageCopy(images[j]);
                    inputPack[inputPackCount].size = images[j].width;
                    inputPack[inputPackCount].valid = true;

                    // NOTE: inputPack[inputPackCount].texture NOT required!

                    printf("LOADED (size: %i)", inputPack[inputPackCount].size);

                    inputPackCount++;
                }

                UnloadImage(images[j]);
                printf("\n");
            }

            free(images);
            free(inputFiles[i]);    // Free input file name memory
        }

        free(inputFiles);           // Free input file names array memory

        // Get bigger available input image
        int biggerSizeIndex = 0;
        int biggerSize = inputPack[0].size;

        for (int i = 1; i < inputPackCount; i++)
        {
            if (inputPack[i].size > biggerSize)
            {
                biggerSize = inputPack[i].size;
                biggerSizeIndex = i;
            }
        }

        printf("\nAll input images processed.\n");
        printf("Images added to the pack: %i (%i", inputPackCount, inputPack[0].size);
        for (int i = 1; i < inputPackCount; i++) printf(",%i", inputPack[i].size);
        printf(")\n");
        printf("Biggest size available: %i\n\n", biggerSize);

        printf(" > PROCESSING OUTPUT FILE\n\n");

        // Generate output sizes list by platform scheme
        switch (outPlatform)
        {
            case ICON_PLATFORM_WINDOWS: for (int i = 0; i < 8; i++) { outSizes[outSizesCount] = icoSizesWindows[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_FAVICON: for (int i = 0; i < 10; i++) { outSizes[outSizesCount] = icoSizesFavicon[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_ANDROID: for (int i = 0; i < 10; i++) { outSizes[outSizesCount] = icoSizesAndroid[i]; outSizesCount++; }; break;
            case ICON_PLATFORM_IOS7: for (int i = 0; i < 9; i++) { outSizes[outSizesCount] = icoSizesiOS[i]; outSizesCount++; }; break;
            default: return;
        }

        IconPackEntry *outPack = NULL;
        int outPackCount = 0;

        if (outSizesCount > 0)
        {
            printf("Output sizes requested: %i", outSizes[0]);
            for (int i = 1; i < outSizesCount; i++) printf(",%i", outSizes[i]);
            printf("\n");

            // Generate custom sizes if required, use biggest available input size and use provided scale algorythm
            outPackCount = outSizesCount;
            outPack = (IconPackEntry *)malloc(outPackCount*sizeof(IconPackEntry));

            // Copy from inputPack or generate if required
            for (int i = 0; i < outPackCount; i++)
            {
                outPack[i].size = outSizes[i];

                // Check input pack for size to copy
                for (int j = 0; j < inputPackCount; j++)
                {
                    if (outPack[i].size == inputPack[j].size)
                    {
                        printf(" > Size %i: COPIED from input images.\n", outPack[i].size);
                        outPack[i].image = ImageCopy(inputPack[j].image);
                        outPack[i].valid = true;
                        break;
                    }
                }

                // Generate image size if not copied
                if (!outPack[i].valid)
                {
                    printf(" > Size %i: GENERATED from input bigger image (%i).\n", outPack[i].size, biggerSize);
                    outPack[i].image = ImageCopy(inputPack[biggerSizeIndex].image);

                    if (scaleAlgorythm == 1) ImageResizeNN(&outPack[i].image, outPack[i].size, outPack[i].size);
                    else if (scaleAlgorythm == 2) ImageResize(&outPack[i].image, outPack[i].size, outPack[i].size);

                    outPack[i].valid = true;
                }
            }

            printf("\n");

            // Save output icon file
            Image *outImages = (Image *)calloc(outPackCount, sizeof(Image));
            for (int i = 0; i < outPackCount; i++) outImages[i] = outPack[i].image;
            SaveICO(outImages, outPackCount, outFileName);
            free(outImages);
        }
        else printf("WARNING: No output sizes defined\n");

        // Extract required images: all or provided sizes (only available ones)
        if (extractAll)
        {
            // Extract all input pack images
            for (int i = 0; i < inputPackCount; i++)
            {
                if (inputPack[i].valid)
                {
                    printf(" > Image extract requested (%i): %s_%ix%i.png\n", inputPack[i].size, GetFileNameWithoutExt(outFileName), inputPack[i].size, inputPack[i].size);
                    ExportImage(inputPack[i].image, TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), inputPack[i].size, inputPack[i].size));
                }
            }
        }
        else if (extractSize)
        {
            // Extract requested sizes from pack (if available)
            for (int i = 0; i < inputPackCount; i++)
            {
                for (int j = 0; j < extractSizesCount; j++)
                {
                    if (inputPack[i].size == extractSizes[j])
                    {
                        printf(" > Image extract requested (%i): %s_%ix%i.png\n", extractSizes[j], GetFileNameWithoutExt(outFileName), inputPack[i].size, inputPack[i].size);
                        ExportImage(inputPack[i].image, TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(outFileName), inputPack[i].size, inputPack[i].size));
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
        for (int i = 0; i < inputPackCount; i++) UnloadImage(inputPack[i].image);
        for (int i = 0; i < outPackCount; i++) UnloadImage(outPack[i].image);
        free(outPack);
    }

    if (showUsageInfo) ShowCommandLineInfo();
}
#endif      // VERSION_ONE || COMMAND_LINE_ONLY

//--------------------------------------------------------------------------------------------
// Load/Save/Export functions
//--------------------------------------------------------------------------------------------
#if !defined(COMMAND_LINE_ONLY)
// Load icon file into an image array
// NOTE: Uses global variables: icoSizesPlatform
static void LoadIconToPack(IconPack *pack, const char *fileName)
{
    int imCount = 0;
    Image *images = NULL;

    // Load all available images
    if (IsFileExtension(fileName, ".ico")) images = LoadICO(fileName, &imCount);
    else if (IsFileExtension(fileName, ".png"))
    {
        imCount = 1;
        images = (Image *)malloc(imCount*sizeof(Image));
        images[0] = LoadImage(fileName);
    }

    // Process all loaded images
    for (int i = 0; i < imCount; i++)
    {
        int index = -1;

        // Check if provided image size is valid (only squared images supported)
        if (images[i].width != images[i].height) printf("WARNING: Image is not squared as expected (%i x %i)\n", images[i].width, images[i].height);
        else
        {
            // Validate loaded images for current platform
            for (int k = 0; k < pack->count; k++)
            {
                if (images[i].width == pack->sizes[k]) { index = k; break; }
            }
        }

        // Load image into pack slot only if it's empty
        if ((index >= 0) && !pack->icons[index].valid)
        {
            // Force image to be RGBA
            ImageFormat(&images[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

            // Re-load image/texture from ico pack
            UnloadImage(pack->icons[index].image);
            UnloadTexture(pack->icons[index].texture);

            pack->icons[index].image = ImageCopy(images[i]);
            pack->icons[index].texture = LoadTextureFromImage(pack->icons[index].image);
            pack->icons[index].size = pack->sizes[index];
            pack->icons[index].valid = true;
        }
        else
        {
            printf("WARNING: Image size not supported (%i x %i)\n", images[i].width, images[i].height);

            // TODO: Re-scale image to the closer supported ico size
            //ImageResize(&image, newWidth, newHeight);
        }

        UnloadImage(images[i]);
    }

    free(images);
}

// Initialize icon pack for an specific platform
// NOTE: Uses globals: sizeTextList
static IconPack LoadIconPack(int platform)
{
    IconPack pack = { 0 };

    switch (platform)
    {
        case ICON_PLATFORM_WINDOWS: pack.count = 8; pack.sizes = icoSizesWindows; break;
        case ICON_PLATFORM_FAVICON: pack.count = 10; pack.sizes = icoSizesFavicon; break;
        case ICON_PLATFORM_ANDROID: pack.count = 10; pack.sizes = icoSizesAndroid; break;
        case ICON_PLATFORM_IOS7: pack.count = 9; pack.sizes = icoSizesiOS; break;
        default: break;
    }

    pack.icons = (IconPackEntry *)malloc(pack.count*sizeof(IconPackEntry));

    // Generate placeholder images
    for (int i = 0; i < pack.count; i++)
    {
        pack.icons[i].size = pack.sizes[i];
        pack.icons[i].image = GenImageColor(pack.icons[i].size, pack.icons[i].size, DARKGRAY);
        ImageDrawRectangle(&pack.icons[i].image, 1, 1, pack.icons[i].size - 2, pack.icons[i].size - 2, GRAY);

        pack.icons[i].texture = LoadTextureFromImage(pack.icons[i].image);
        pack.icons[i].valid = false;
    }

    return pack;
}

// Unload icon pack
static void UnloadIconPack(IconPack *pack)
{
    if ((pack != NULL) && (pack->count > 0))
    {
        for (int i = 0; i < pack->count; i++)
        {
            UnloadImage(pack->icons[i].image);
            UnloadTexture(pack->icons[i].texture);
        }
    }
}

// Unload one icon from the pack
// NOTE: A placeholder image is re-generated
static void UnloadIconFromPack(IconPack *pack, int index)
{
    if (pack->icons[index].valid)
    {
        UnloadImage(pack->icons[index].image);
        UnloadTexture(pack->icons[index].texture);

        pack->icons[index].image = GenImageColor(pack->icons[index].size, pack->icons[index].size, DARKGRAY);
        ImageDrawRectangle(&pack->icons[index].image, 1, 1, pack->icons[index].size - 2, pack->icons[index].size - 2, GRAY);
        pack->icons[index].texture = LoadTextureFromImage(pack->icons[index].image);
        pack->icons[index].valid = false;
    }
}

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
        length = TextLength(TextFormat("%i x %i;", pack.sizes[i], pack.sizes[i]));
        memcpy(buffer + offset, TextFormat("%i x %i;", pack.sizes[i], pack.sizes[i]), length);
        offset += length;
    }

    buffer[offset - 1] = '\0';

    return buffer;
}
#endif      // !COMMAND_LINE_ONLY

// Icon data loader
// NOTE: Returns an array of images
static Image *LoadICO(const char *fileName, int *count)
{
    Image *images = NULL;

    FILE *icoFile = fopen(fileName, "rb");

    if (icoFile != NULL)
    {
        // Load .ico information
        IcoHeader icoHeader = { 0 };
        fread(&icoHeader, 1, sizeof(IcoHeader), icoFile);

        images = (Image *)malloc(icoHeader.imageCount*sizeof(Image));
        *count = icoHeader.imageCount;

        IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.imageCount, sizeof(IcoDirEntry));

        for (int i = 0; i < icoHeader.imageCount; i++) fread(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

        for (int i = 0; i < icoHeader.imageCount; i++)
        {
            unsigned char *icoImageData = (unsigned char *)malloc(icoDirEntry[i].size);
            fread(icoImageData, icoDirEntry[i].size, 1, icoFile);    // Read icon image data

            // Reading image data from memory buffer
            // WARNING: Image data on th IcoDirEntry may be in either:
            //  - Windows BMP format, excluding the BITMAPFILEHEADER structure
            //  - PNG format, stored in its entirety
            // NOTE: We are only supporting the PNG format
            // TODO: Support BMP data
            ((Image *)images)[i] = LoadImageFromMemory(".png", icoImageData, icoDirEntry[i].size);

            free(icoImageData);
        }

        free(icoDirEntry);

        fclose(icoFile);
    }

    return images;
}

// Icon saver
// NOTE: Make sure images array sizes are valid!
static void SaveICO(Image *images, int imageCount, const char *fileName)
{
    IcoHeader icoHeader = { .reserved = 0, .imageType = 1, .imageCount = imageCount };
    IcoDirEntry *icoDirEntry = (IcoDirEntry *)calloc(icoHeader.imageCount, sizeof(IcoDirEntry));

    char **icoData = (char **)malloc(icoHeader.imageCount*sizeof(char *));     // Pointers array to PNG image data
    int offset = 6 + 16*icoHeader.imageCount;

    for (int i = 0; i < imageCount; i++)
    {
        int fileSize = 0;       // Store generated png file size
        int colorChannels = 0;

        // Compress images data into PNG file data streams
        // Image data format could be RGB (3 bytes) instead of RGBA (4 bytes)
        if (images[i].format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) colorChannels = 3;
        else if (images[i].format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) colorChannels = 4;

        // NOTE: Memory is allocated internally using RPNG_MALLOC(), must be freed with RPNG_FREE()
        icoData[i] = rpng_save_image_to_memory(images[i].data, images[i].width, images[i].height, colorChannels, 8, &fileSize);

        icoDirEntry[i].width = (images[i].width == 256)? 0 : images[i].width;
        icoDirEntry[i].height = (images[i].width == 256)? 0 : images[i].width;
        icoDirEntry[i].bpp = 32;
        icoDirEntry[i].size = fileSize;
        icoDirEntry[i].offset = offset;

        offset += fileSize;
    }

    FILE *icoFile = fopen(fileName, "wb");

    if (icoFile != NULL)
    {
        // Write ico header
        fwrite(&icoHeader, 1, sizeof(IcoHeader), icoFile);

        // Write icon images entries data
        for (int i = 0; i < icoHeader.imageCount; i++) fwrite(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

        // Write icon png data
        for (int i = 0; i < icoHeader.imageCount; i++) fwrite(icoData[i], 1, icoDirEntry[i].size, icoFile);

        fclose(icoFile);
    }

    // Free used data
    for (int i = 0; i < icoHeader.imageCount; i++) RPNG_FREE(icoData[i]);
    free(icoDirEntry);
    free(icoData);
}

/*
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
    
    // ICNS support a long list of OSType icon data formats,
    // we will only support and load a small subset and **only PNG data format**:
    //  OSType  Size        Details
    //---------------------------------------------------------------------------
    // - icp4	16x16	    JPEG 2000† or PNG† format or 24-bit RGB icon[2]
    // - icp5	32x32	    JPEG 2000† or PNG† format or 24-bit RGB icon[2]
    // - icp6	48x48	    JPEG 2000† or PNG† format
    // - ic07	128x128	    JPEG 2000 or PNG format
    // - ic08	256x256	    JPEG 2000 or PNG format
    // - ic09	512x512	    JPEG 2000 or PNG format
    // - ic10	1024x1024   JPEG 2000 or PNG format (512x512@2x "retina" in 10.8)
    // - ic11	32x32	    JPEG 2000 or PNG format (16x16@2x "retina")
    // - ic12	64x64	    JPEG 2000 or PNG format (32x32@2x "retina")
    // - ic13	256x256	    JPEG 2000 or PNG format (128x128@2x "retina")
    // - ic14	512x512	    JPEG 2000 or PNG format (256x256@2x "retina")
    
    // App icon sizes (recommended)
    // https://developer.apple.com/design/human-interface-guidelines/macos/icons-and-images/app-icon/
    // - 512x512 (512x512@1x, 1024x1024@2x)
    // - 256x256 (256x256@1x, 512x512@2x)
    // - 128x128 (128x128@1x, 256x256@2x)
    // - 32x32   (32x32@1x, 64x64@2x)
    // - 16x16   (16x16@1x, 32x32@2x)

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
*/