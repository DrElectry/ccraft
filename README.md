## ccraft

![Demo](pictures/frame.gif)

![Demo](pictures/clouds.gif)

![Demo](pictures/building.png)

CCraft is a work-in-progress sandbox minecraft like voxel game, written fully in C with OpenGL and a couple of lib dependencies.

### Goals

The goal of this project is to have a flexible, cross platform, easily moddable game with stable multiplayer support suitable for low end devices.

### Status

After 4 months of work on the project i already have some prototypes for HUD, deferred rendering, the "core minecraft" part and some item system experiments for future inventory system.

The engine is still too fragile and unoptimized for all of my needs, specifically major refactoring of the chunks system and management is required for project to progress, i still haven't fully establised at least some idea of the visual part of the project.

Multiplayer aspect of the game requires major refactoring too, for example generating chunks on the server, client prediction, and a bit of tweaks are ABSOLUTELY REQUIRED!

I also got some laptop issues lately, which can delay the development.

### Things i already implemented

- Core "minecraft clone part" (chunks, placing and breaking blocks, player movement with physics, collisions, infinite world generation, lightmaps, etc),
- Deferred rendering,
- HUD,
- Sounds, ambient,
- WIP Multiplayer.

Early prototypes for later features:

- Skeletal animations,
- Soft body experimental implementation,

### Planned features

The list is huge, but things i need to implement ASAP:

- Day-Night cycle,
- Decide the visual style of the game,
- Rigidbody physics,
- More optimizations,
- GUI, or at least some idea of it.

### Multiplayer

![Demo](pictures/multiplayer_demo.gif)

As i said, multiplayer is under construction, it is still very buggy, but if you want to try it:

- Run the "server" binary, everything configurable is avaliable inside file "server.properties".
- Connect to the server using the -connect <IP:PORT> flag.
- You can also use 'localhost' as the ip.
- Custom nickname can be setupped using the -nickname <NAME> flag, otherwise the nickname will be created automatically.
- Chat is avaliable on the server by using the T key, pressing ENTER will send your message to the server from your name.
- Enjoy.

The server runs on 32 TPS with interpolation.

### License

MIT license, because sharing is caring.

### Contribution

Anyone can contribute or fork the project, any help will be greeted with open hands.

## CREDITS

Music: “Taswell” by C418 (from Minecraft: Volume Beta)

Sound effects are from Minecraft by Mojang Studios.
Minecraft is a trademark of Mojang Studios.
