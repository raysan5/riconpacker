<img align="left" src="logo/riconpacker_256x256.png" width=256>

# rIconPacker

A simple and easy-to-use icons packer and extractor.

Pack/Unpack icon files, load icon and add new image sizes or just export icon images. 

rGuiIcons can be used for free as a [WebAssembly online tool](https://raylibtech.itch.io/riconpacker) and it can also be downloaded as a **standalone tool** for _Windows_ and _Linux_ with some extra features.

<br>

## rIconPacker Features

 - **Pack** your custom images into an icon
 - Input image formats supported: `.bmp`, `.png`, `.jpg` and `.qoi`
 - **Unpack** icon to check available image sizes
 - **Generate** missing icon sizes for selected platform
 - **Extract icon images** as PNG files
 - **4 predefined platforms: Windows, Favicon, Android, iOS**

### rIconPacker Standalone Additional Features

 - Command-line support for icons packing and extraction
 - Command-line supports configurable image scaling algorythm
 - **Completely portable (single-file, no-dependencies)**

## rIconPacker Screenshot

![rIconPacker](screenshots/riconpacker_v100_shot02.png)

## rIconPacker Usage

Drag & drop your ICO/PNG files to unpack/add the different available image sizes, missing sizes could also be generated (using biggest available size).

`rIconPacker Standalone` comes with command-line support for batch conversion. For usage help:

 > riconpacker.exe --help

## Building rIconPacker
In order to build rIconpacker, you'll need raylib installed. On windows, install using the official installer, on linux, either build from source or use the package manager of choice.

Building on linux
```sh
cd src
gcc -o riconpacker riconpacker.c external/tinyfiledialogs.c -s -Iexternal -no-pie -D_DEFAULT_SOURCE -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

Building on windows
```cmd
cd src
gcc -o riconpacker.exe riconpacker.c external/tinyfiledialogs.c -s riconpacker.rc.data -Iexternal -lraylib -lopengl32 -lgdi32 -lcomdlg32 -lole32 -std=c99 -Wall
```


## rIconPacker License

`rIconPacker` source code is distributed as **open source**, licensed under an unmodified [zlib/libpng license](LICENSE). 

`rIconPacker` binaries are completely free for anyone willing to compile it directly from source.

`rIconPacker Standalone` desktop tool is distributed as freeware. 

In any case, consider some donation to help the author keep working on software for games development.

*Copyright (c) 2018-2022 raylib technologies ([@raylibtech](https://twitter.com/raylibtech)) / Ramon Santamaria ([@raysan5](https://twitter.com/raysan5))*
