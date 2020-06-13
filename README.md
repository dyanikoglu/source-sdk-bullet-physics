# source-sdk-bullet-physics
Bullet Physics Injection for Source SDK 2013.

## About

The repository is based on https://github.com/DrChat/Gmod-vphysics. It's not forked because of having whole Bullet SDK pushed into same repository to keep this repository crisp & clear. Major changes on this implementation are;

- Updated version of Bullet SDK. The base repositry had an ancient version of Bullet with lots of deprecated stuff being used.
- Utilized new multithreaded modules of Bullet
- Lots of performance improvements

## Demonstration

https://www.youtube.com/watch?v=6CH1BSyM92A

https://www.youtube.com/watch?v=kMz_1qonMqs

## Building Vphysics.dll for Source SDK 2013

- Checkout submodules inside thirdparty folder
- Follow the instructions on bullet3 repository to build Bullet libs & binaries
- Open vphysics solution and build the project
- Place generated vphysics.dll binary into desired Source SDK 2013 based game (Half-Life 2, GMod etc.), or into your custom built Source SDK 2013 game.

## Known Issues
- Save/Load functionality doesn't work, and mostly crashes the game. You should disable physics restore functionality on save/load module of Source SDK 2013 to fix this issue.
- Small objects with very high speed (Thrown grenades for example) may pass through from landscape mesh. Also, big objects with very high speed may have a tunnelling effect while colliding with landscape meshes. That's mostly an issue with Bullet's messed up convex mesh collision algorithm, and it's not likely to be solved because of core part of the physics engine being abondoned on development.
- Impact damage is not implemented. Because of Bullet's lack of a proper callback system for hit/impact/collision events, it requires huge amount of effort while using multithreaded modules of Bullet.
- Physics impact sounds are broken, again, it will probably require huge amount of effort to be fixed.
- The implementation is highly experimental, it's not recommended to use it with production purposes.
