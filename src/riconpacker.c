/*******************************************************************************************
*
*   rIconPacker v2.0 - A simple and easy-to-use icons packer
*
*   CONFIGURATION:
*
*   #define COMMAND_LINE_ONLY
*       Compile tool only for command line usage
*
*   #define CUSTOM_MODAL_DIALOGS
*       Use custom raygui generated modal dialogs instead of native OS ones
*       NOTE: Avoids including tinyfiledialogs depencency library
*
*   VERSIONS HISTORY:
*       2.0  (04-Oct-2022)  ADDED: Support text info data for every icon image
*                           ADDED: Export icon images as a .zip package
*                           ADDED: Main toolbar to access File/Tools/Visual options
*                           ADDED: Help window with keyboard shortcuts info
*                           ADDED: Multiple new gui styles
*                           REDESIGNED: UI to follow raylibtech UI conventions
*                           Updated to raylib 4.2 and raygui 3.2
*                           Source code re-licensed to open-source
*       1.5  (30-Dec-2021)  Updated to raylib 4.0 and raygui 3.1
*       1.0  (23-Mar-2019)  First release
*
*   DEPENDENCIES:
*       raylib 4.2              - Windowing/input management and drawing
*       raygui 3.2              - Immediate-mode GUI controls with custom styling and icons
*       rpng 1.0                - PNG chunks management
*       tinyfiledialogs 3.8.8   - Open/save file dialogs, it requires linkage with comdlg32 and ole32 libs
*       miniz 2.2.0             - Save .zip package file (required for multiple images export)
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
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2018-2022 raylib technologies (@raylibtech) / Ramon Santamaria (@raysan5)
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
#define TOOL_VERSION            "2.0"
#define TOOL_DESCRIPTION        "A simple and easy-to-use icons packer"
#define TOOL_RELEASE_DATE       "Oct.2022"
#define TOOL_LOGO_COLOR         0xffc800ff

#include "raylib.h"

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                         // Required for: IMGUI controls

#undef RAYGUI_IMPLEMENTATION                // Avoid including raygui implementation again

#define GUI_WINDOW_ABOUT_IMPLEMENTATION
#include "gui_window_about.h"               // GUI: About Window

#define GUI_FILE_DIALOGS_IMPLEMENTATION
#include "gui_file_dialogs.h"               // GUI: File Dialogs

#define GUI_MAIN_TOOLBAR_IMPLEMENTATION
#include "gui_main_toolbar.h"               // GUI: Main toolbar

// raygui embedded styles
// NOTE: Inclusion order follows combobox order
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

#include "external/miniz.h"
#include "external/miniz.c"

#include <stdio.h>                          // Required for: fopen(), fclose(), fread()...
#include <stdlib.h>                         // Required for: calloc(), free()
#include <string.h>                         // Required for: strcmp(), strlen()
#include <math.h>                           // Required for: ceil()

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

#define MAX_IMAGE_TEXT_SIZE  64

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Icon File Header (6 bytes)
typedef struct {
    unsigned short reserved;    // Must always be 0.
    unsigned short imageType;   // Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image. Other values are invalid.
    unsigned short imageCount;  // Specifies number of entries in the file.
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
    char text[MAX_IMAGE_TEXT_SIZE]; // Text to be embedded in the image
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

#define HELP_LINES_COUNT    14

// Tool help info
static const char *helpLines[HELP_LINES_COUNT] = {
    "F1 - Show Help window",
    "F2 - Show About window",
    "-File Controls",
    "LCTRL + N - New icon file (.ico)",
    "LCTRL + O - Open icon/image file (.ico/.png)",
    "LCTRL + S - Save icon file (.ico)",
    "LCTRL + E - Export icon/image file(s)",
    "-Tool Controls",
    "SUP - Remove selected icon image",
    "LCTRL + G - Generate selected icon image",
    "-Tool Visuals",
    "RIGHT - Select style template",
    NULL,
    "ESCAPE - Close Window/Exit"
};

// NOTE: Default icon sizes by platform: http://iconhandbook.co.uk/reference/chart/
static unsigned int icoSizesWindows[8] = { 256, 128, 96, 64, 48, 32, 24, 16 };              // Windows app icons
static unsigned int icoSizesFavicon[10] = { 228, 152, 144, 120, 96, 72, 64, 32, 24, 16 };   // Favicon for multiple devices
static unsigned int icoSizesAndroid[10] = { 192, 144, 96, 72, 64, 48, 36, 32, 24, 16 };     // Android Launcher/Action/Dialog/Others icons, missing: 512
static unsigned int icoSizesiOS[9] = { 180, 152, 120, 87, 80, 76, 58, 40, 29 };             // iOS App/Settings/Others icons, missing: 512, 1024

#if !defined(COMMAND_LINE_ONLY)
static IconPack packs[4] = { 0 };           // Icon packs, one for every platform
static int sizeListActive = 0;              // Current list text entry
static int validCount = 0;                  // Valid ico entries counter
#endif

static bool exportImagesChecked = true;     // Flag to export entries separated
static bool exportTextChunkChecked = true;  // Flag to embed text as a PNG chunk (rIPt)

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#if defined(PLATFORM_DESKTOP) || defined(COMMAND_LINE_ONLY)
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

#if !defined(COMMAND_LINE_ONLY)
// Icon pack management functions
static IconPack InitIconPack(int platform);     // Load icon pack for a specific platform
static void CloseIconPack(IconPack *pack);     // Unload icon pack

static void LoadIconToPack(IconPack *pack, const char *fileName); // Load icon file into pack
static void UnloadIconFromPack(IconPack *pack, int index);        // Unload one icon from the pack

static char *GetTextIconSizes(IconPack pack);   // Get sizes as a text array separated by semicolon (ready for GuiListView())
#endif

// Load/Save/Export data functions
static IconPackEntry *LoadICO(const char *fileName, int *count);    // Load icon data
static void SaveICO(IconPackEntry *entries, int entryCount, const char *fileName, bool imageOnly);  // Save icon data

// Draw help window with the provided lines
static int GuiHelpWindow(Rectangle bounds, const char *title, const char **helpLines, int helpLinesCount);

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
                IsFileExtension(argv[1], ".png;.bmp;.jpg;.qoi"))
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
    FreeConsole();
#endif

    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 400;
    const int screenHeight = 380;

    InitWindow(screenWidth, screenHeight, TextFormat("%s v%s", toolName, toolVersion));
    SetExitKey(0);

    // General pourpose variables
    //Vector2 mousePos = { 0.0f, 0.0f };
    //int frameCounter = 0;

    // Initialize all icon packs (for all platforms)
    packs[0] = InitIconPack(ICON_PLATFORM_WINDOWS);
    packs[1] = InitIconPack(ICON_PLATFORM_FAVICON);
    packs[2] = InitIconPack(ICON_PLATFORM_ANDROID);
    packs[3] = InitIconPack(ICON_PLATFORM_IOS7);

    // GUI: Main Layout
    //-----------------------------------------------------------------------------------
    Vector2 anchorMain = { 0, 0 };

    int scaleAlgorythmActive = 1;

    bool btnGenIconImagePressed = false;
    bool btnClearIconImagePressed = false;

    bool iconTextEditMode = false;

    bool screenSizeActive = false;
    bool helpWindowActive = false;      // Show window: help info
    bool userWindowActive = false;      // Show window: user registration

    GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 24);
    //-----------------------------------------------------------------------------------

    // GUI: About Window
    //-----------------------------------------------------------------------------------
    GuiWindowAboutState windowAboutState = InitGuiWindowAbout();
    //-----------------------------------------------------------------------------------

    // GUI: Main toolbar panel (file and visualization)
    //-----------------------------------------------------------------------------------
    GuiMainToolbarState mainToolbarState = InitGuiMainToolbar();
    //-----------------------------------------------------------------------------------

    // GUI: Export Window
    //-----------------------------------------------------------------------------------
    bool exportWindowActive = false;
    int exportFormatActive = 0;         // ComboBox file type selection (.ico, .png)
    //-----------------------------------------------------------------------------------

    // GUI: Exit Window
    //-----------------------------------------------------------------------------------
    bool closeWindow = false;
    bool exitWindowActive = false;
    //-----------------------------------------------------------------------------------

    // GUI: Custom file dialogs
    //-----------------------------------------------------------------------------------
    bool showLoadFileDialog = false;
    bool showExportFileDialog = false;
    //bool showExportImageDialog = false;
    //-----------------------------------------------------------------------------------

    // Check if an icon input file has been provided on command line
    if (inFileName[0] != '\0') LoadIconToPack(&packs[mainToolbarState.platformActive], inFileName);

    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!closeWindow)    // Detect window close button
    {
        // WARNING: ASINCIFY requires this line,
        // it contains the call to emscripten_sleep() for PLATFORM_WEB
        if (WindowShouldClose()) exitWindowActive = true;

        // Dropped files logic
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();

            // Support gui styles
            if ((droppedFiles.count == 1) && IsFileExtension(droppedFiles.paths[0], ".rgs")) GuiLoadStyle(droppedFiles.paths[0]);

            for (int i = 0; i < droppedFiles.count; i++)
            {
                if (IsFileExtension(droppedFiles.paths[i], ".ico") ||
                    IsFileExtension(droppedFiles.paths[i], ".png;.bmp;.jpg;.qoi"))
                {
                    // Load entries into IconPack
                    LoadIconToPack(&packs[mainToolbarState.platformActive], droppedFiles.paths[i]);
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
            CloseIconPack(&packs[mainToolbarState.platformActive]);
            packs[mainToolbarState.platformActive] = InitIconPack(mainToolbarState.platformActive);
        }

        // Show dialog: load input file (.ico, .png)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) showLoadFileDialog = true;

        // Show dialog: save icon file (.ico)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E))
        {
            if (validCount > 0)
            {
                strcpy(outFileName, "icon.ico");
                showExportFileDialog = true;
            }
        }

        // Show dialog: export icon data
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
        {
            exportFormatActive = 0;         // Icon (.ico)
            exportImagesChecked = false;
            exportTextChunkChecked = true;
            strcpy(outFileName, "icon.ico");
            showExportFileDialog = true;
        }

#if defined(PLATFORM_DESKTOP)
        // Toggle screen size (x2) mode
        //if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_F)) screenSizeActive = !screenSizeActive;
#endif

        // Toggle window help
        if (IsKeyPressed(KEY_F1)) helpWindowActive = !helpWindowActive;

        // Toggle window about
        if (IsKeyPressed(KEY_F2)) windowAboutState.windowActive = !windowAboutState.windowActive;

        // Toggle window registered user
        //if (IsKeyPressed(KEY_F3)) userWindowActive = !userWindowActive;

        // Delete selected icon from list
        if (IsKeyPressed(KEY_DELETE) || btnClearIconImagePressed)
        {
            if (sizeListActive == 0) for (int i = 0; i < packs[mainToolbarState.platformActive].count; i++) UnloadIconFromPack(&packs[mainToolbarState.platformActive], i);  // Delete all entries in the series
            else UnloadIconFromPack(&packs[mainToolbarState.platformActive], sizeListActive - 1);                                            // Delete one image
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
            else if (helpWindowActive) helpWindowActive = false;
            else if (exportWindowActive) exportWindowActive = false;
        #if defined(PLATFORM_DESKTOP)
            else exitWindowActive = !exitWindowActive;
        #else
            else if (showLoadFileDialog) showLoadFileDialog = false;
            else if (showExportFileDialog) showExportFileDialog = false;
        #endif
        }

        // Change current style template
        if (IsKeyPressed(KEY_RIGHT)) mainToolbarState.btnStylePressed = true;
        //----------------------------------------------------------------------------------

        // Main toolbar logic
        //----------------------------------------------------------------------------------
        // File options logic
        if (mainToolbarState.btnLoadFilePressed) showLoadFileDialog = true;
        else if (mainToolbarState.btnSaveFilePressed)
        {
            exportFormatActive = 0;         // Icon (.ico)
            exportImagesChecked = false;
            exportTextChunkChecked = true;
            strcpy(outFileName, "icon.ico");
            showExportFileDialog = true;
        }
        else if (mainToolbarState.btnExportFilePressed) exportWindowActive = true;

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
        if (mainToolbarState.btnHelpPressed) helpWindowActive = true;               // Help button logic
        if (mainToolbarState.btnAboutPressed) windowAboutState.windowActive = true; // About window button logic
        if (mainToolbarState.btnUserPressed) userWindowActive = true;               // User button logic
        //----------------------------------------------------------------------------------

        // Basic program flow logic
        //----------------------------------------------------------------------------------
        // Calculate valid entries
        validCount = 0;
        for (int i = 0; i < packs[mainToolbarState.platformActive].count; i++) if (packs[mainToolbarState.platformActive].icons[i].valid) validCount++;

        // Generate new icon image (using biggest available image)
        if ((IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) || btnGenIconImagePressed)
        {
            // Get bigger available icon
            int biggerValidSize = -1;
            for (int i = 0; i < packs[mainToolbarState.platformActive].count; i++)
            {
                if (packs[mainToolbarState.platformActive].icons[i].valid) { biggerValidSize = i; break; }
            }

            if (biggerValidSize >= 0)
            {
                if (sizeListActive == 0)
                {
                    // Generate all missing entries in the series
                    for (int i = 0; i < packs[mainToolbarState.platformActive].count; i++)
                    {
                        if (!packs[mainToolbarState.platformActive].icons[i].valid)
                        {
                            UnloadImage(packs[mainToolbarState.platformActive].icons[i].image);
                            packs[mainToolbarState.platformActive].icons[i].image = ImageCopy(packs[mainToolbarState.platformActive].icons[biggerValidSize].image);

                            if (scaleAlgorythmActive == 0) ImageResizeNN(&packs[mainToolbarState.platformActive].icons[i].image, packs[mainToolbarState.platformActive].icons[i].size, packs[mainToolbarState.platformActive].icons[i].size);
                            else if (scaleAlgorythmActive == 1) ImageResize(&packs[mainToolbarState.platformActive].icons[i].image, packs[mainToolbarState.platformActive].icons[i].size, packs[mainToolbarState.platformActive].icons[i].size);

                            UnloadTexture(packs[mainToolbarState.platformActive].icons[i].texture);
                            packs[mainToolbarState.platformActive].icons[i].texture = LoadTextureFromImage(packs[mainToolbarState.platformActive].icons[i].image);

                            packs[mainToolbarState.platformActive].icons[i].valid = true;
                        }
                    }
                }
                else
                {
                    if (!packs[mainToolbarState.platformActive].icons[sizeListActive - 1].valid)
                    {
                        UnloadImage(packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image);
                        packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image = ImageCopy(packs[mainToolbarState.platformActive].icons[biggerValidSize].image);

                        if (scaleAlgorythmActive == 0) ImageResizeNN(&packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image, packs[mainToolbarState.platformActive].icons[sizeListActive - 1].size, packs[mainToolbarState.platformActive].icons[sizeListActive - 1].size);
                        else if (scaleAlgorythmActive == 1) ImageResize(&packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image, packs[mainToolbarState.platformActive].icons[sizeListActive - 1].size, packs[mainToolbarState.platformActive].icons[sizeListActive - 1].size);

                        UnloadTexture(packs[mainToolbarState.platformActive].icons[sizeListActive - 1].texture);
                        packs[mainToolbarState.platformActive].icons[sizeListActive - 1].texture = LoadTextureFromImage(packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image);

                        packs[mainToolbarState.platformActive].icons[sizeListActive - 1].valid = true;
                    }
                }
            }
        }

        // Change active platform icons pack
        if (mainToolbarState.platformActive != mainToolbarState.prevPlatformActive)
        {
            // TODO: Check icons that can be populated from current platform to next platform

            mainToolbarState.prevPlatformActive = mainToolbarState.platformActive;
        }
        //----------------------------------------------------------------------------------

        // Screen scale logic (x2) -> Not used in this tool
        //----------------------------------------------------------------------------------
        /*
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
        */
        //----------------------------------------------------------------------------------


        // WARNING: Some windows should lock the main screen controls when shown
        if (windowAboutState.windowActive ||
            helpWindowActive ||
            userWindowActive ||
            exitWindowActive ||
            exportWindowActive ||
            showLoadFileDialog ||
            showExportFileDialog) GuiLock();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            // GUI: Main Layout: List view and icons viewer panel
            //--------------------------------------------------------------------------------------------------------------
            sizeListActive = GuiListView((Rectangle) { anchorMain.x + 10, anchorMain.y + 52, 115, 290 }, GetTextIconSizes(packs[mainToolbarState.platformActive]), NULL, sizeListActive);
            if (sizeListActive < 0) sizeListActive = 0;

            GuiDummyRec((Rectangle){ anchorMain.x + 135, anchorMain.y + 52, 256, 256 }, NULL);
            DrawRectangleLines((int)anchorMain.x + 135, (int)anchorMain.y + 52, 256, 256, Fade(GRAY, 0.6f));

            if (sizeListActive == 0)
            {
                for (int i = 0; i < packs[mainToolbarState.platformActive].count; i++) DrawTexture(packs[mainToolbarState.platformActive].icons[i].texture, (int)anchorMain.x + 135, (int)anchorMain.y + 52, WHITE);
            }
            else if (sizeListActive > 0)
            {
                DrawTexture(packs[mainToolbarState.platformActive].icons[sizeListActive - 1].texture,
                            (int)anchorMain.x + 135 + 128 - packs[mainToolbarState.platformActive].icons[sizeListActive - 1].texture.width/2,
                            (int)anchorMain.y + 52 + 128 - packs[mainToolbarState.platformActive].icons[sizeListActive - 1].texture.height/2, WHITE);
            }

            // Clear/generate selected icon image level
            // NOTE: Enabled buttons depend on several circunstances
            if ((validCount == 0) || ((sizeListActive > 0) && !packs[mainToolbarState.platformActive].icons[sizeListActive - 1].valid)) GuiDisable();
            btnClearIconImagePressed = GuiButton((Rectangle){ anchorMain.x + 135 + 256 - 48 - 8, anchorMain.y + 52 + 256 - 24 - 4, 24, 24 }, "#143#");
            GuiEnable();

            if ((validCount == 0) || ((sizeListActive > 0) && packs[mainToolbarState.platformActive].icons[sizeListActive - 1].valid)) GuiDisable();
            btnGenIconImagePressed = GuiButton((Rectangle){ anchorMain.x + 135 + 256 - 24 - 4, anchorMain.y + 52 + 256 - 24 - 4, 24, 24 }, "#142#");
            GuiEnable();

            // Icon image text for embedding
            if (sizeListActive == 0) GuiDisable();
            if (GuiTextBox((Rectangle){ anchorMain.x + 135, anchorMain.y + 52 + 256 + 8, 256, 26 }, (sizeListActive == 0)? "Add custom image text here!" : packs[mainToolbarState.platformActive].icons[sizeListActive - 1].text, MAX_IMAGE_TEXT_SIZE, iconTextEditMode)) iconTextEditMode = !iconTextEditMode;
            GuiEnable();
            //--------------------------------------------------------------------------------------------------------------

            // GUI: Main toolbar panel
            //----------------------------------------------------------------------------------
            GuiMainToolbar(&mainToolbarState);
            //----------------------------------------------------------------------------------

            // GUI: Status bar
            //----------------------------------------------------------------------------------------
            GuiStatusBar((Rectangle){ anchorMain.x + 0, GetScreenHeight() - 24, 130, 24 }, (sizeListActive == 0) ? "IMAGE: ALL" : TextFormat("IMAGE: %ix%i", packs[mainToolbarState.platformActive].icons[sizeListActive - 1].size, packs[mainToolbarState.platformActive].icons[sizeListActive - 1].size));
            GuiStatusBar((Rectangle){ anchorMain.x + 130 - 1, GetScreenHeight() - 24, screenWidth - 129, 24 }, (sizeListActive > 0)? TextFormat("IMAGE TEXT LENGTH: %i/%i", strlen(packs[mainToolbarState.platformActive].icons[sizeListActive - 1].text), MAX_IMAGE_TEXT_SIZE - 1) : NULL);
            //----------------------------------------------------------------------------------------

            // NOTE: If some overlap window is open and main window is locked, we draw a background rectangle
            if (GuiIsLocked()) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.85f));

            // WARNING: Before drawing the windows, we unlock them
            GuiUnlock();

            // GUI: About Window
            //----------------------------------------------------------------------------------------
            GuiWindowAbout(&windowAboutState);
            //----------------------------------------------------------------------------------------

            // GUI: Help Window
            //----------------------------------------------------------------------------------------
            Rectangle helpWindowBounds = { (float)screenWidth/2 - 330/2, (float)screenHeight/2 - 368.0f/2, 330, 0 };
            if (helpWindowActive) helpWindowActive = GuiHelpWindow(helpWindowBounds, GuiIconText(ICON_HELP, TextFormat("%s Shortcuts", TOOL_NAME)), helpLines, HELP_LINES_COUNT);
            //----------------------------------------------------------------------------------------

            // GUI: Export Window
            //----------------------------------------------------------------------------------------
            if (exportWindowActive)
            {
                Rectangle messageBox = { (float)screenWidth/2 - 248/2, (float)screenHeight/2 - 200/2, 248, 164 };
                int result = GuiMessageBox(messageBox, "#7#Export Icon File", " ", "#7#Export Icon");

                GuiLabel((Rectangle){ messageBox.x + 12, messageBox.y + 12 + 24, 106, 24 }, "Icon Format:");
                exportFormatActive = GuiComboBox((Rectangle){ messageBox.x + 12 + 88, messageBox.y + 12 + 24, 136, 24 }, "Icon (.ico);Images (.png)", exportFormatActive);

                if (exportFormatActive == 1) { exportImagesChecked = true; GuiDisable(); }
                exportImagesChecked = GuiCheckBox((Rectangle){ messageBox.x + 20, messageBox.y + 48 + 24, 16, 16 }, "Export individual PNG images", exportImagesChecked);
                GuiEnable();
                exportTextChunkChecked = GuiCheckBox((Rectangle){ messageBox.x + 20, messageBox.y + 72 + 24, 16, 16 }, "Embed image text as rIPt chunk", exportTextChunkChecked);

                if (result == 1)    // Export button pressed
                {
                    exportWindowActive = false;
                    strcpy(outFileName, "icon.ico");
                    showExportFileDialog = true;
                }
                else if (result == 0) exportWindowActive = false;
            }
            //----------------------------------------------------------------------------------

            // GUI: Exit Window
            //----------------------------------------------------------------------------------------
            if (exitWindowActive)
            {
                int result = GuiMessageBox((Rectangle){ GetScreenWidth()/2.0f - 125, GetScreenHeight()/2.0f - 50, 250, 100 }, "#159#Closing rIconPacker", "Do you really want to exit?", "Yes;No");

                if ((result == 0) || (result == 2)) exitWindowActive = false;
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
                int result = GuiFileDialog(DIALOG_OPEN_FILE, "Load icon or image file...", inFileName, "*.ico;*.png", "Icon or Image Files (*.ico, *.png)");
#endif
                if (result == 1) LoadIconToPack(&packs[mainToolbarState.platformActive], inFileName);   // Load icon file

                if (result >= 0) showLoadFileDialog = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export File Dialog (and saving logic)
            //----------------------------------------------------------------------------------------
            if (showExportFileDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiTextInputBox((Rectangle){ screenWidth/2 - 280/2, screenHeight/2 - 112/2 - 30, 280, 112 }, (exportFormatActive == 0)? "#7#Export icon file..." : "#7#Export image files...", NULL, "#7#Export", outFileName, 512, NULL);
#else
                int result = -1;
                if (exportFormatActive == 0) result = GuiFileDialog(DIALOG_SAVE_FILE, "Export icon file...", outFileName, "*.ico", "Icon File (*.ico)");
                else result = GuiFileDialog(DIALOG_SAVE_FILE, "Export image files...", outFileName, "*.png", "Image Files (*.png)");
#endif
                if (result == 1)
                {
                    // Check for valid extension and make sure it is
                    if ((GetFileExtension(outFileName) == NULL) && !IsFileExtension(outFileName, ".ico")) strcat(outFileName, ".ico\0");

                    // Save into icon file provided pack entries
                    // NOTE: Only valid entries are exported, png zip packaging also done (if required)
                    SaveICO(packs[mainToolbarState.platformActive].icons, packs[mainToolbarState.platformActive].count, outFileName, exportFormatActive);

                #if defined(PLATFORM_WEB)
                    // Download file from MEMFS (emscripten memory filesystem)
                    // NOTE: Second argument must be a simple filename (we can't use directories)
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", outFileName, GetFileName(outFileName)));

                    if (exportImagesChecked)
                    {
                        char tempFileName[512] = { 0 };
                        strcpy(tempFileName, TextFormat("%s.zip", outFileName));
                        emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", tempFileName, GetFileName(tempFileName)));
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
                strcpy(outFileName, TextFormat("icon_%ix%i.png", packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image.width, packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image.height));

#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_TEXTINPUT, "Export image file...", outFileName, "Ok;Cancel", NULL);
#else
                int result = GuiFileDialog(DIALOG_SAVE_FILE, "Export image file...", outFileName, "*.png", "Image File (*.png)");
#endif
                if (result == 1)
                {
                    // Check for valid extension and make sure it is
                    if ((GetFileExtension(outFileName) == NULL) || !IsFileExtension(outFileName, ".png")) strcat(outFileName, ".png\0");

                    ExportImage(packs[mainToolbarState.platformActive].icons[sizeListActive - 1].image, outFileName);

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

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload icon packs data
    for (int i = 0; i < 4; i++) CloseIconPack(&packs[i]);

    CloseWindow();      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

#endif      // !COMMAND_LINE_ONLY

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module functions definition
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
                const char **files = TextSplit(argv[i + 1], ',', &inputFilesCount);

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

        IconPackEntry inputPack[MAX_ICONS_PACK] = { 0 }; // Icon entries array
        int inputPackCount = 0;                          // Icon entries array counter

        printf(" > PROCESSING INPUT FILES\n");

        // Load input files (all of them) into icon pack, if one size has been previously loaded, do not load again
        for (int i = 0; i < inputFilesCount; i++)
        {
            int imCount = 0;
            IconPackEntry *entries = NULL;

            // Load all available entries in current file
            if (IsFileExtension(inputFiles[i], ".ico")) entries = LoadICO(inputFiles[i], &imCount);
            else if (IsFileExtension(inputFiles[i], ".png;.bmp;.jpg;.qoi"))
            {
                imCount = 1;
                entries = (IconPackEntry *)RL_CALLOC(imCount, sizeof(IconPackEntry));
                entries[0].image = LoadImage(inputFiles[i]);
            }

            printf("\nInput file: %s / Images loaded: %i\n", inputFiles[i], imCount);

            // Process all loaded entries
            for (int j = 0; j < imCount; j++)
            {
                printf(" > Processing image: %i ", j);

                // Check if provided image size is valid (only squared entries supported)
                if (entries[j].image.width != entries[j].image.height) printf("WARNING: Image is not squared as expected (%i x %i)", entries[j].image.width, entries[j].image.height);
                else
                {
                    // TODO: Check if current image size is already available in the package!

                    // Force image to be RGBA
                    ImageFormat(&entries[j].image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

                    inputPack[inputPackCount].image = ImageCopy(entries[j].image);
                    //inputPack[inputPackCount].texture = LoadTextureFromImage(entries[j].image);   // Not required on the command-line
                    inputPack[inputPackCount].size = entries[j].image.width;
                    inputPack[inputPackCount].valid = true;
                    memcpy(inputPack[inputPackCount].text, entries[j].text, strlen(entries[j].text));

                    // NOTE: inputPack[inputPackCount].texture NOT required!

                    printf("LOADED (size: %i)", inputPack[inputPackCount].size);

                    inputPackCount++;
                }

                UnloadImage(entries[j].image);
                printf("\n");
            }

            RL_FREE(entries);
            RL_FREE(inputFiles[i]);    // Free input file name memory
        }

        RL_FREE(inputFiles);           // Free input file names array memory

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
            outPack = (IconPackEntry *)RL_CALLOC(outPackCount, sizeof(IconPackEntry));

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

            // Save into icon file provided pack entries
            // NOTE: Only valid entries are exported, png zip packaging also done (if required)
            SaveICO(outPack, outPackCount, outFileName, false);
        }
        else printf("WARNING: No output sizes defined\n");

        // Extract required entries: all or provided sizes (only available ones)
        if (extractAll)
        {
            // Extract all input pack entries
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
        RL_FREE(outPack);
    }

    if (showUsageInfo) ShowCommandLineInfo();
}
#endif

//--------------------------------------------------------------------------------------------
// Load/Save/Export functions
//--------------------------------------------------------------------------------------------
#if !defined(COMMAND_LINE_ONLY)
// Initialize icon pack for an specific platform
// NOTE: Uses globals: sizeTextList
static IconPack InitIconPack(int platform)
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

    pack.icons = (IconPackEntry *)RL_CALLOC(pack.count, sizeof(IconPackEntry));

    // Generate placeholder entries
    for (int i = 0; i < pack.count; i++)
    {
        pack.icons[i].size = pack.sizes[i];
        pack.icons[i].image = GenImageColor(pack.icons[i].size, pack.icons[i].size, DARKGRAY);
        ImageDrawRectangle(&pack.icons[i].image, 1, 1, pack.icons[i].size - 2, pack.icons[i].size - 2, GRAY);

        pack.icons[i].texture = LoadTextureFromImage(pack.icons[i].image);
        pack.icons[i].valid = false;
        //memset(pack.icons[i].text, 0, MAX_IMAGE_TEXT_SIZE);
    }

    return pack;
}

// Unload icon pack
static void CloseIconPack(IconPack *pack)
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

// Load icon file into an image array
// NOTE: Uses global variables: icoSizesPlatform
static void LoadIconToPack(IconPack *pack, const char *fileName)
{
    int imCount = 0;
    IconPackEntry *entries = NULL;

    // Load all available entries
    if (IsFileExtension(fileName, ".ico")) entries = LoadICO(fileName, &imCount);
    else if (IsFileExtension(fileName, ".png;.bmp;.jpg;.qoi"))
    {
        imCount = 1;
        entries = (IconPackEntry *)RL_CALLOC(imCount, sizeof(IconPackEntry));
        entries[0].image = LoadImage(fileName);
    }

    // Process all loaded entries
    for (int i = 0; i < imCount; i++)
    {
        int index = -1;

        // Check if provided image size is valid (only squared entries supported)
        if (entries[i].image.width != entries[i].image.height) printf("WARNING: Image is not squared as expected (%i x %i)\n", entries[i].image.width, entries[i].image.height);
        else
        {
            // Validate loaded entries for current platform
            for (int k = 0; k < pack->count; k++)
            {
                if (entries[i].image.width == pack->sizes[k]) { index = k; break; }
            }
        }

        // Load image into pack slot only if it's empty
        if ((index >= 0) && !pack->icons[index].valid)
        {
            // Force image to be RGBA
            ImageFormat(&entries[i].image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

            // Re-load image/texture from ico pack
            UnloadImage(pack->icons[index].image);
            UnloadTexture(pack->icons[index].texture);

            pack->icons[index].image = ImageCopy(entries[i].image);
            pack->icons[index].texture = LoadTextureFromImage(pack->icons[index].image);
            pack->icons[index].size = pack->sizes[index];
            pack->icons[index].valid = true;
            memcpy(pack->icons[index].text, entries[i].text, strlen(entries[i].text));
        }
        else
        {
            printf("WARNING: Image size not supported (%i x %i)\n", entries[i].image.width, entries[i].image.height);

            // TODO: Re-scale image to the closer supported ico size
            //ImageResize(&image, newWidth, newHeight);
        }

        UnloadImage(entries[i].image);
    }

    RL_FREE(entries);
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
// NOTE: Returns an array of entries
static IconPackEntry *LoadICO(const char *fileName, int *count)
{
    IconPackEntry *entries = NULL;

    FILE *icoFile = fopen(fileName, "rb");

    if (icoFile != NULL)
    {
        // Load .ico information
        IcoHeader icoHeader = { 0 };
        fread(&icoHeader, 1, sizeof(IcoHeader), icoFile);

        entries = (IconPackEntry *)RL_CALLOC(icoHeader.imageCount, sizeof(IconPackEntry));
        *count = icoHeader.imageCount;

        IcoDirEntry *icoDirEntry = (IcoDirEntry *)RL_CALLOC(icoHeader.imageCount, sizeof(IcoDirEntry));

        for (int i = 0; i < icoHeader.imageCount; i++) fread(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

        for (int i = 0; i < icoHeader.imageCount; i++)
        {
            char *icoImageData = (char *)RL_CALLOC(icoDirEntry[i].size, 1);
            fread(icoImageData, icoDirEntry[i].size, 1, icoFile);    // Read icon image data

            // Reading image data from memory buffer
            // WARNING: Image data on th IcoDirEntry may be in either:
            //  - Windows BMP format, excluding the BITMAPFILEHEADER structure
            //  - PNG format, stored in its entirety
            // NOTE: We are only supporting the PNG format
            // TODO: Support BMP data
            entries[i].image = LoadImageFromMemory(".png", icoImageData, icoDirEntry[i].size);
            entries[i].size = entries[i].image.width;   // Icon size (expected squared)
            entries[i].valid = false;                   // Not valid until it is checked against the current package (sizes)

            // Read custom rIconPacker text chunk from PNG
            rpng_chunk chunk = rpng_chunk_read_from_memory(icoImageData, "rIPt");
            memcpy(entries[i].text, chunk.data, (chunk.length < MAX_IMAGE_TEXT_SIZE)? chunk.length : MAX_IMAGE_TEXT_SIZE - 1);
            RPNG_FREE(chunk.data);

            RL_FREE(icoImageData);
        }

        RL_FREE(icoDirEntry);

        fclose(icoFile);
    }

    return entries;
}

// Icon saver
// NOTE: Make sure entries array sizes are valid!
static void SaveICO(IconPackEntry *entries, int entryCount, const char *fileName, bool imageOnly)
{
    // Verify icon pack valid entries (not placeholder ones)
    int validCount = 0;
    for (int i = 0; i < entryCount; i++) if (entries[i].valid) validCount++;

    if (validCount == 0) return;

    // Define ico file header and entry
    IcoHeader icoHeader = { .reserved = 0, .imageType = 1, .imageCount = validCount };
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

            // Check if exporting png entries is required (as a .zip)
            if (exportImagesChecked)
            {
                // Package every image into an output ZIP file (fileName.zip)
                // NOTE: A comment is optional text information that is embedded in a Zip file
                mz_bool status = mz_zip_add_mem_to_archive_file_in_place(TextFormat("%s.zip", fileName), TextFormat("%s_%ix%i.png", GetFileNameWithoutExt(fileName), entries[i].image.width, entries[i].image.height), pngDataPtrs[i], fileSize, "This is a comment", strlen("This is a comment"), MZ_BEST_SPEED); //MZ_BEST_COMPRESSION, MZ_DEFAULT_COMPRESSION
                if (!status) LOG("WARNING: Zip accumulation process failed\n");
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

    if (!imageOnly)     // In case not images only export required, pack .ico
    {
        FILE *icoFile = fopen(fileName, "wb");

        if (icoFile != NULL)
        {
            // Write ico header
            fwrite(&icoHeader, 1, sizeof(IcoHeader), icoFile);

            // Write icon entries entries data
            for (int i = 0; i < icoHeader.imageCount; i++) fwrite(&icoDirEntry[i], 1, sizeof(IcoDirEntry), icoFile);

            // Write icon png data
            for (int i = 0; i < icoHeader.imageCount; i++) fwrite(pngDataPtrs[i], 1, icoDirEntry[i].size, icoFile);

            fclose(icoFile);
        }
    }

    // Free used data (pngs data)
    for (int i = 0; i < icoHeader.imageCount; i++) RPNG_FREE(pngDataPtrs[i]);

    RL_FREE(icoDirEntry);
    RL_FREE(pngDataPtrs);
}

// Draw help window with the provided lines
static int GuiHelpWindow(Rectangle bounds, const char *title, const char **helpLines, int helpLinesCount)
{
    int nextLineY = 0;

    // Calculate window height if not externally provided a desired height
    if (bounds.height == 0) bounds.height = (float)(helpLinesCount*24 + 24);

    int helpWindowActive = !GuiWindowBox(bounds, title);
    nextLineY += (24 + 2);

    for (int i = 0; i < helpLinesCount; i++)
    {
        if (helpLines[i] == NULL) GuiLine((Rectangle){ bounds.x, bounds.y + nextLineY, 330, 12 }, helpLines[i]);
        else if (helpLines[i][0] == '-') GuiLine((Rectangle){ bounds.x, bounds.y + nextLineY, 330, 24 }, helpLines[i] + 1);
        else GuiLabel((Rectangle){ bounds.x + 12, bounds.y + nextLineY, bounds.width, 24 }, helpLines[i]);

        if (helpLines[i] == NULL) nextLineY += 12;
        else nextLineY += 24;
    }

    return helpWindowActive;
}

/*
// Apple ICNS icons loader
// NOTE: Format specs: https://en.wikipedia.org/wiki/Apple_Icon_Image_format
static Image *LoadICNS(const char *fileName, int *count)
{
    Image *entries = NULL;

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
    // - icp4    16x16        JPEG 2000† or PNG† format or 24-bit RGB icon[2]
    // - icp5    32x32        JPEG 2000† or PNG† format or 24-bit RGB icon[2]
    // - icp6    48x48        JPEG 2000† or PNG† format
    // - ic07    128x128        JPEG 2000 or PNG format
    // - ic08    256x256        JPEG 2000 or PNG format
    // - ic09    512x512        JPEG 2000 or PNG format
    // - ic10    1024x1024   JPEG 2000 or PNG format (512x512@2x "retina" in 10.8)
    // - ic11    32x32        JPEG 2000 or PNG format (16x16@2x "retina")
    // - ic12    64x64        JPEG 2000 or PNG format (32x32@2x "retina")
    // - ic13    256x256        JPEG 2000 or PNG format (128x128@2x "retina")
    // - ic14    512x512        JPEG 2000 or PNG format (256x256@2x "retina")

    // App icon sizes (recommended)
    // https://developer.apple.com/design/human-interface-guidelines/macos/icons-and-entries/app-icon/
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
    entries = (Image *)RL_CALLOC(icnsCount, 1);
    *count = icnsCount;

    fclose(icnsFile);

    return entries;
}
*/