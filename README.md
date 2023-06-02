# rIconPacker

A simple and easy-to-use icons packer and extractor.

## What can I do with rIconPacker?

Pack/Unpack icon files, load .ico/.icns files and view all contained images, load individual .bmp/.png/.qoi images to create new icons.
Select desired platform for automatic icon-sizes template, provide required icons or generate them from loaded sizes.

Create icon poems! Every icon image support up to 47 text characters for embedding. Add icons poems that get exported with the icon/images and share them with your friends.

## Features

 - **Icon template platforms**: Windows, macOS, Favicon, Android, iOS
 - **Pack** your icon images into icon files (`.ico`/`icns`)
 - Define **custom text data** per icon image: icon poems
 - **Generate** missing icon sizes automatically
 - Input image formats supported: `.png`, `.bmp`, `.qoi`
 - **Extract icon images** as `.png` files
 - Multiple GUI styles with support for custom ones (`.rgs`)
 - Command-line support for icons packing and extraction
 - Command-line supports configurable image scaling algorithms
 - **Completely portable (single-file, no-dependencies)**

## Basic Usage

Drag & drop your `.ico`/`.icns`/`.png` files to unpack/add the different available image sizes, missing sizes could also be generated (using biggest available size).

**Custom image text data** can be added per icon image and it will be embedded in the `.ico`/`.icns`/`.png` file, useful for copyright data or nice _icon-poems_. 

`rIconPacker` can be used from its desktop UI application and also from its command-line interface.

### Keyboard/Mouse Shortcuts

 - `F1` - Show Help window
 - `F2` - Show About window
 - `F3` - Show Sponsor window

**File Options**
 - `LCTRL + N` - New icon project
 - `LCTRL + O` - Open icon/image file (.ico/.icns/.png)
 - `LCTRL + S` - Save icon file (.ico/.icns/.png)
 - `LCTRL + E` - Export icon/image file(s)

**Tool Options**
 - `DELETE` - Remove selected icon image
 - `LCTRL + G` - Generate selected icon image
 - `LCTRL + SPACE` - Show icon-poem (if available)

### Command-line

`rIconPacker` desktop version comes with command-line support for batch conversion. For usage help:

 > riconpacker.exe --help

```
  USAGE:\n
    > riconpacker [--help] --input <file01.ext>,[file02.ext],... [--output <filename.ico>]
                  [--out-sizes <size01>,[size02],...] [--out-platform <value>] [--scale-algorythm <value>]
                  [--extract-size <size01>,[size02],...] [--extract-all]

  OPTIONS:\n
    -h, --help                      : Show tool version and command line usage help
    -i, --input <file01.ext>,[file02.ext],...
                                    : Define input file(s). Comma separated for multiple files.
                                      Supported extensions: .ico, .icns, .png, .bmp, .qoi
    -o, --output <filename.ico>     : Define output icon file.
                                      NOTE: If not specified, defaults to: output.ico
    -op, --out-platform <value>     : Define out sizes by platform scheme.
                                      Supported values:
                                          0 - Windows (Sizes: 256, 128, 96, 64, 48, 32, 24, 16)
                                          1 - macOS (Sizes: 1024, 512, 256, 128, 64, 48, 32, 16)
                                          2 - favicon (Sizes: 228, 152, 144, 120, 96, 72, 64, 32, 24, 16)
                                          3 - Android (Sizes: 192, 144, 96, 72, 64, 48, 36, 32, 24, 16)
                                          4 - iOS (Sizes: 180, 152, 120, 87, 80, 76, 58, 40, 29)
                                      NOTE: If not specified, any icon size can be generated
    -os, --out-sizes <size01>,[size02],...
                                    : Define output sizes for the output.
                                      If output size is not provided as input, it's generated.
                                      NOTE: Generated icons are always squared.
    -sa, --scale-algorythm <value>  : Define the algorythm used to scale images.
                                      Supported values:
                                          1 - Nearest-neighbor scaling algorythm
                                          2 - Bicubic scaling algorythm (default)
    -xs, --extract-size <size01>,[size02],...
                                    : Extract image sizes from input (if size is available)
                                      NOTE: Exported images name: output_{size}.png
    -xa, --extract-all              : Extract all images from icon.
                                      NOTE: Exported images naming: output_{size}.png,...
```

## Technologies

This tool has been created using the following open-source technologies:

 - [raylib](https://github.com/raysan5/raylib) - A simple and easy-to-use library to enjoy videogames programming
 - [raygui](https://github.com/raysan5/raygui) - A simple and easy-to-use immediate-mode-gui library
 - [rpng](https://github.com/raysan5/rpng) - A simple and easy-to-use library to library to manage png chunks
 - [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) - File dialogs for desktop platforms

## Handmade Software

rIconPacker is handmade software, it has been meticulously developed using the C programming language, with great attention put on each line of code written.
This approach usually results in highly optimized and efficient code, but it is also more time-consuming and require a higher level of technical skills.
The result is great performance and small memory footprint.

rIconpacker is self-contained in a single-executable of about **1 MB**, it could fit on a floppy disk.

rIconpacker could be customized on demand for corporate users, if your company needs a custom version, just get in touch: ray[at]raylibtech.com

## Issues & Feedback

You can report tool issues and feedback at https://github.com/raylibtech/rtools

## License

The use of `rIconPacker` distributed desktop application is subject to the terms and conditions of the `End User License Agreement`.
By using `rIconPacker`, the user agrees to be bound by the terms of the EULA.

Check included `End User License Agreement` document for details (EULA.txt).

`rIconPacker` source code is distributed as **open source**, licensed under an unmodified [zlib/libpng license](LICENSE). 

`rIconPacker` binaries are completely free for anyone willing to compile it directly from sources.

In any case, consider some donation to help the author keep working on useful software development.

*Copyright (c) 2018-2023 raylib technologies ([@raylibtech](https://twitter.com/raylibtech)) | Ramon Santamaria ([@raysan5](https://twitter.com/raysan5))*
