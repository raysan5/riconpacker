<img align="left" src="logo/riconpacker_256x256.png" width=256>

# rIconPacker

A simple and easy-to-use icons packer and extractor.

Pack/Unpack icon files, load icon and add new image sizes or just export icon images. 

rGuiIcons can be used for free as a [WebAssembly online tool](https://raylibtech.itch.io/riconpacker) and it can also be downloaded as a **standalone tool** for _Windows_ and _Linux_ with some extra features.

<br>

## rIconPacker Features

 - **Icon template platforms**: Windows, Favicon, Android, iOS
 - **Pack** your icon images into an icon file (`.ico`)
 - Define **custom text data** per icon image
 - **Generate** missing icon sizes automatically
 - Input image formats supported: `.png`, `.bmp`, `.jpg` and `.qoi`
 - **Extract icon images** as `.png` files
 - Multiple GUI styles with support for custom ones (`.rgs`)
 - **Completely free and open-source**

### rIconPacker Standalone Additional Features

 - Command-line support for icons packing and extraction
 - Command-line supports configurable image scaling algorythm
 - **Completely portable (single-file, no-dependencies)**

## rIconPacker Screenshot

![rIconPacker](screenshots/riconpacker_v200_shot01.png)

## rIconPacker Usage

Drag & drop your ICO/PNG files to unpack/add the different available image sizes, missing sizes could also be generated (using biggest available size).

**Custom image text data** can be added per icon image and it will be embedded in the `.ico` file, useful for copyright data or nice _icon-poems_. 

`rIconPacker Standalone` comes with command-line support for batch conversion. For usage help:

 > riconpacker.exe --help

## rIconPacker License

`rIconPacker` source code is distributed as **open source**, licensed under an unmodified [zlib/libpng license](LICENSE). 

`rIconPacker` binaries are completely free for anyone willing to compile it directly from source.

`rIconPacker Standalone` desktop tool can be downloaded with a small donation.

In any case, consider some donation to help the author keep working on software for games development.

*Copyright (c) 2018-2022 raylib technologies ([@raylibtech](https://twitter.com/raylibtech)) / Ramon Santamaria ([@raysan5](https://twitter.com/raysan5))*
