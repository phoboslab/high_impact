
	██╗  ██╗██╗ ██████╗ ██╗  ██╗        ██╗███╗   ███╗██████╗  █████╗  ██████╗████████╗
	▓▓║  ▓▓║▓▓║▓▓╔════╝ ▓▓║  ▓▓║        ▓▓║▓▓▓▓╗ ▓▓▓▓║▓▓╔══▓▓╗▓▓╔══▓▓╗▓▓╔════╝╚══▓▓╔══╝
	▒▒▒▒▒▒▒║▒▒║▒▒║  ▒▒▒╗▒▒▒▒▒▒▒║        ▒▒║▒▒╔▒▒▒▒╔▒▒║▒▒▒▒▒▒╔╝▒▒▒▒▒▒▒║▒▒║        ▒▒║
	░░╔══░░║░░║░░║   ░░║░░╔══░░║        ░░║░░║╚░░╔╝░░║░░╔═══╝ ░░╔══░░║░░║        ░░║
	░░║  ░░║░░║╚░░░░░░╔╝░░║  ░░║░░░░░░░╗░░║░░║ ╚═╝ ░░║░░║     ░░║  ░░║╚░░░░░░╗   ░░║
	╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝╚═╝     ╚═╝╚═╝     ╚═╝  ╚═╝ ╚═════╝   ╚═╝
	
	░░░░▒▒▒▒▒▒▒▒▒▒▓▓▓▓▓▓▓▓▓▓▓▓██████████████████████████████▓▓▓▓▓▓▓▓▓▓▓▓▒▒▒▒▒▒▒▒▒▒░░░░╗
	╚═════════════════════════════════════════════════════════════════════════════════╝

high_impact is a C game engine for creating 2d action games. It's well suited
for jump'n'runs, twin stick shooters, top-down dungeon crawlers and others with 
a focus on pixel art.

This is NOT a general purpose game engine akin to Godot, Unreal or Unity. At
this stage, it is also quite experimental and lacking documentation. Expect
problems.

high_impact is more a framework than it is a library, meaning that you have to
adhere to a structure, in code and file layout, that is prescribed by the 
engine. You do not call high_impact, high_impact _calls you_.

Games made with high_impact can be compiled for Linux, macOS, Windows (through 
the usual hoops) and for the web with WASM. There are currently two "platform 
backends": SDL2 & Sokol and two different renderers: OpenGL and a rudimentary 
software renderer.

Please read the accompanying blog post [Porting my JavaScript Game Engine to 
C for no reason](https://phoboslab.org/log/2024/08/high_impact) for
some insights into the decisions made here.


## Examples

- [Biolab Disaster](https://github.com/phoboslab/high_biolab): A jump'n'gun 
platformer, displaying many of high_impacts capabilities.
- [Drop](https://github.com/phoboslab/high_drop): A minimal arcade game with
randomly generated levels


## Compiling

No Makefile is provided here as high_impact must be compiled together with your
game. It can not be compiled as a standalone library. 

It's best to start your development with one of the examples above as the basis.
These examples come with a Makefile that should _just work_™


## Documentation

There's not much at the moment. Most of high_impact's functionality is 
documented in the header files with this README giving a general overview.
It's best to read [the blog post](https://phoboslab.org/log/2024/08/high_impact)
for an overview and the source for all the details: It's just ~4000 lines of 
code.


## Assets

At this time, high_impact can only load images in QOI format and sounds & music 
in QOA format. The tools to convert PNG to QOI and WAV to QOA are bundled in 
this repository and can be integrated in your build step. See the example games 
for a suitable Makefile.

Game levels can be loaded from .json files. A tile editor to create these levels
is part of high_impact: `weltmeister.html` which can be launched with a simple
double click from your local copy.


## Libraries used

- SDL2: https://github.com/libsdl-org/SDL
- Sokol App, Audio and Time: https://github.com/floooh/sokol
- glad: https://github.com/Dav1dde/glad
- stb_image.h and stb_image_write.h https://github.com/nothings/stb
- QOI Image Format: https://github.com/phoboslab/qoi
- QOA Audio Format: https://github.com/phoboslab/qoa
- pl_json: https://github.com/phoboslab/pl_json

Except for SDL2, all libraries are bundled here (see the `libs/` directory).


## License

All high_impact code is MIT Licensed, though some of the libraries (`libs/`) 
come with their own (permissive) license. Check the header files.
